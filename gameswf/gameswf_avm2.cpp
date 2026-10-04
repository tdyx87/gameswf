// gameswf_avm2.cpp	-- Vitaly Alexeev <tishka92@yahoo.com>	2008

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// AVM2 implementation

#include "gameswf/gameswf_avm2.h"
#include "gameswf/gameswf_stream.h"
#include "gameswf/gameswf_log.h"
#include "gameswf/gameswf_abc.h"
#include "gameswf/gameswf_disasm.h"
#include "gameswf/gameswf_character.h"
#include "gameswf/gameswf_sprite.h"
#include "gameswf_jit.h"
#include "gameswf/gameswf_as_classes/as_array.h"
#include "gameswf/gameswf_as_classes/as_class.h"
#include "gameswf/gameswf_as_classes/as_graphics.h"

namespace gameswf
{

	// True when the ABC class described by ii derives, up to two levels, from a
	// built-in display object class.  Mirrors the check done by constructprop.
	static bool avm2_class_extends_display(abc_def* abc, instance_info* ii)
	{
		if (ii == NULL || ii->m_super_name <= 0)
		{
			return false;
		}
		const char* super_name = abc->get_multiname(ii->m_super_name);
		if (super_name == NULL)
		{
			return false;
		}
		if (strcmp(super_name, "Sprite") == 0 || strcmp(super_name, "MovieClip") == 0 ||
			strcmp(super_name, "SimpleButton") == 0 || strcmp(super_name, "TextField") == 0 ||
			strcmp(super_name, "DisplayObject") == 0 || strcmp(super_name, "DisplayObjectContainer") == 0 ||
			strcmp(super_name, "InteractiveObject") == 0 ||
			strcmp(super_name, "Shape") == 0)
		{
			return true;
		}
		instance_info* super_ii = abc->get_instance_info(tu_string(super_name));
		if (super_ii == NULL || super_ii->m_super_name <= 0)
		{
			return false;
		}
		const char* grandparent = abc->get_multiname(super_ii->m_super_name);
		return grandparent != NULL && (
			strcmp(grandparent, "Sprite") == 0 || strcmp(grandparent, "MovieClip") == 0 ||
			strcmp(grandparent, "SimpleButton") == 0 || strcmp(grandparent, "TextField") == 0 ||
			strcmp(grandparent, "DisplayObject") == 0 || strcmp(grandparent, "DisplayObjectContainer") == 0 ||
			strcmp(grandparent, "InteractiveObject") == 0 ||
			strcmp(grandparent, "Shape") == 0);
	}

	// Which object a function-valued property was read from.  AS3 method
	// references (e.g. `loader.addEventListener(IO_ERROR, ioErrorHandler)`)
	// must keep their receiver: when the listener later runs, `this` has to
	// be the object the method was taken from, not the event target.
	static hash<as_function*, as_value>& avm2_method_owners()
	{
		static hash<as_function*, as_value> s_owners;
		return s_owners;
	}

	static void avm2_note_method_owner(as_object* obj, const as_value& val)
	{
		if (obj == NULL || val.is_function() == false)
		{
			return;
		}
		as_value owner;
		owner.set_as_object(obj);
		as_function* fn = val.to_function();
		{
			static char s_seen[1024][32];
			static void* s_owner[1024];
			static int s_n = 0;
			char key[32];
			snprintf(key, sizeof(key), "%p/%p", (void*)fn, (void*)obj);
			int hit = -1;
			for (int i = 0; i < s_n; i++)
			{
				if (strcmp(s_seen[i], key) == 0) { hit = i; break; }
			}
			if (hit < 0 && s_n < 1024)
			{
				s_owner[s_n] = obj;
				strncpy(s_seen[s_n], key, 31);
				s_seen[s_n][31] = 0;
				s_n++;
				fprintf(stderr, "[MOWN] fn=%p obj=%p\n", (void*)fn, (void*)obj);
				fflush(stderr);
			}
		}
		avm2_method_owners().set(fn, owner);
	}

	void avm2_clear_method_owners()
	{
		avm2_method_owners().clear();
	}

	// Deliver evt to every listener registered on target under "__events_<type>".
	// The listener list is the same one addEventListener() fills in.
	void avm2_dispatch_event(as_object* target, const as_value& evt, as_environment* env)
	{
		if (target == NULL || env == NULL)
		{
			return;
		}

		// Determine the event type. AS3 event instances carry it in "type";
		// a plain string argument is used as-is.
		tu_string type_str;
		as_value type_val;
		as_object* evt_obj = evt.to_object();
		if (evt_obj && evt_obj->get_member("type", &type_val) && !type_val.is_undefined())
		{
			type_str = type_val.to_string();
		}
		else
		{
			type_str = evt.to_string();
		}

		if (type_str == "")
		{
			return;
		}

		tu_string event_key = "__events_";
		event_key += type_str;

		as_value listeners_val;
		if (!target->get_member(event_key, &listeners_val) || !listeners_val.is_object())
		{
			fprintf(stderr, "[DISPATCH] target=%p type='%s' no listeners\n", target, type_str.c_str());
			fflush(stderr);
			return;
		}

		as_array* listeners_array = cast_to<as_array>(listeners_val.to_object());
		if (listeners_array == NULL)
		{
			return;
		}

		fprintf(stderr, "[DISPATCH] target=%p type='%s' count=%d\n",
			target, type_str.c_str(), listeners_array->size());
		fflush(stderr);

		// Copy first: a listener may add/remove listeners while we iterate.
		array<as_value> snapshot;
		snapshot.resize(listeners_array->size());
		for (int i = 0; i < listeners_array->size(); i++)
		{
			snapshot[i] = listeners_array->m_array[i];
		}

		for (int i = 0; i < (int)snapshot.size(); i++)
		{
			as_value listener = snapshot[i];
			if (listener.is_function())
			{
				// Prefer the object the method was read from as `this`; fall
				// back to the dispatch target for plain closures.
				as_object* this_ptr = target;
				as_value owner;
				bool has_owner = avm2_method_owners().get(listener.to_function(), &owner)
					&& owner.is_object() && owner.to_object() != NULL;
				if (has_owner)
				{
					this_ptr = owner.to_object();
				}
				fprintf(stderr, "[MLISTEN] fn=%p target=%p owner=%p this=%p\n",
					(void*)listener.to_function(), (void*)target,
					has_owner ? (void*)this_ptr : NULL, (void*)this_ptr);
				{
					as_object* p = this_ptr;
					int depth = 0;
					while (p != NULL && depth < 8)
					{
						as_value v;
						bool got = p->get_member("dispatchEvent", &v);
						fprintf(stderr, "[MPROTO] d=%d obj=%p hasDispatch=%d\n",
							depth, (void*)p, got ? 1 : 0);
						p = p->get_proto();
						depth++;
					}
					fflush(stderr);
				}
				fflush(stderr);
				env->push(evt);
				call_method(listener, env, this_ptr, 1, env->get_top_index());
				env->drop(1);
			}
		}
	}

	void avm2_as_dispatch_event(const fn_call& fn)
	{
		if (fn.nargs < 1 || fn.this_ptr == NULL)
		{
			fn.result->set_bool(false);
			return;
		}
		avm2_dispatch_event(fn.this_ptr, fn.arg(0), fn.env);
		fn.result->set_bool(true);
	}

	// event key for a type value; mirrors the addEventListener path.
	static tu_string avm2_event_key(const as_value& event_type)
	{
		tu_string key = "__events_";
		if (event_type.is_object() && event_type.to_object() != NULL)
		{
			as_value type_val;
			if (event_type.to_object()->get_member("type", &type_val) && !type_val.is_undefined())
			{
				key += type_val.to_string();
				return key;
			}
		}
		key += event_type.to_string();
		return key;
	}

	static as_array* avm2_get_listeners(as_object* target, const tu_string& event_key, bool create)
	{
		if (target == NULL)
		{
			return NULL;
		}
		as_value listeners_val;
		if (target->get_member(event_key, &listeners_val) && listeners_val.is_object())
		{
			as_array* arr = cast_to<as_array>(listeners_val.to_object());
			if (arr != NULL)
			{
				return arr;
			}
		}
		if (create)
		{
			as_array* arr = new as_array(target->get_player());
			target->set_member(event_key, as_value(arr));
			return arr;
		}
		return NULL;
	}

	bool avm2_remove_event_listener(as_object* target, const as_value& event_type, const as_value& listener)
	{
		tu_string event_key = avm2_event_key(event_type);
		as_array* arr = avm2_get_listeners(target, event_key, false);
		if (arr == NULL)
		{
			return false;
		}
		as_object* remove_obj = listener.to_object();
		if (remove_obj == NULL)
		{
			return false;
		}
		for (int i = 0; i < arr->size(); i++)
		{
			if (arr->m_array[i].to_object() == remove_obj)
			{
				arr->remove(i);
				if (arr->size() == 0)
				{
					target->set_member(event_key, as_value());
				}
				return true;
			}
		}
		return false;
	}

	bool avm2_has_event_listener(as_object* target, const as_value& event_type)
	{
		as_array* arr = avm2_get_listeners(target, avm2_event_key(event_type), false);
		return arr != NULL && arr->size() > 0;
	}

	void avm2_as_remove_event_listener(const fn_call& fn)
	{
		if (fn.nargs < 2 || fn.this_ptr == NULL)
		{
			fn.result->set_undefined();
			return;
		}
		avm2_remove_event_listener(fn.this_ptr, fn.arg(0), fn.arg(1));
	}

	void avm2_as_has_event_listener(const fn_call& fn)
	{
		if (fn.nargs < 1 || fn.this_ptr == NULL)
		{
			fn.result->set_bool(false);
			return;
		}
		fn.result->set_bool(avm2_has_event_listener(fn.this_ptr, fn.arg(0)));
	}

	void avm2_as_will_trigger(const fn_call& fn)
	{
		if (fn.nargs < 1 || fn.this_ptr == NULL)
		{
			fn.result->set_undefined();
			return;
		}
		fn.result->set_bool(avm2_has_event_listener(fn.this_ptr, fn.arg(0)));
	}

	as_3_function::as_3_function(abc_def* abc, int method, player* player) :
		as_function(player),
		m_abc(abc),
		m_return_type( -1 ),
		m_name( -1 ),
		m_flags( 0 ),
		m_method(method),
		m_max_stack( 0 ),
		m_local_count( 0 ),
		m_init_scope_depth( 0 ),
		m_max_scope_depth( 0 )
	{
		m_this_ptr = this;

		// any function MUST have prototype
		builtin_member("prototype", new as_object(player));
	}

	as_3_function::~as_3_function()
	{
	}

	void	as_3_function::operator()(const fn_call& fn)
	// dispatch
	{
		assert(fn.env);

		// try to use caller environment
		// if the caller object has own environment then we use its environment
		as_environment* env = fn.env;
		if (fn.this_ptr)
		{
			if (fn.this_ptr->get_environment())
			{
				env = fn.this_ptr->get_environment();
			}
		}

		// set 'this'
		as_object* this_ptr = env->get_target();
		if (fn.this_ptr)
		{
			this_ptr = fn.this_ptr;

			// m_this_ptr passes the object being built along a constructor
			// chain.  It is a weak_ptr, so it can already be released; taking
			// it unconditionally replaced a perfectly good receiver with NULL,
			// which left 'this' undefined inside the method and silently broke
			// unqualified calls such as stop() (they end up with no receiver,
			// so the timeline never stops and the clip keeps looping).
			as_object* chained_this = this_ptr->m_this_ptr.get_ptr();
			if (chained_this != NULL)
			{
				this_ptr = chained_this;
			}
		}

		// Create local registers.
		array<as_value>	local_register;
		local_register.resize(m_local_count + 1);

		// Register 0 holds the ?this? object. This value is never null.
		assert(this_ptr);
		local_register[0] = this_ptr;

		// Registers 1 through method_info.param_count holds parameter values.
		// If fewer than method_body_info.local_count values are supplied to the call then
		// the remaining values are either the values provided by default value declarations 
		// or the value undefined.
		int actual_arg_count = fn.nargs < (int)m_param_type.size() ? fn.nargs : (int)m_param_type.size();
		for (int i = 0; i < actual_arg_count; i++)
		{
			// A zero value denotes the any (?*?) type.
			//const char* name = m_abc->get_multiname(m_param_type[i]);
			local_register[i + 1] = fn.arg( i );
		}
		// Fill remaining registers with undefined
		for (int i = actual_arg_count; i < (int)m_param_type.size(); i++)
		{
			local_register[i + 1] = as_value();
		}

#ifdef __GAMESWF_ENABLE_JIT__

		if( !m_compiled_code.is_valid() )
		{
			compile();
			m_compiled_code.initialize();
		}

#endif

		if( m_compiled_code.is_valid() )
		{
			try
			{
				m_compiled_code.call< array<as_value>&, vm_stack&, vm_stack&, as_value* >
					(local_register, *env, env->m_scope, fn.result );
			}
			catch( ... )
			{
				log_msg( "jitted code crashed" );
			}
		}
		else
		{
			// keep stack size on entry
			int stack_size = env->size();

			// Push 'this' into the AVM2 scope stack so that findpropstrict/getlex
			// can find methods on the current object (e.g. stop, play, addFrameScript).
			// In a correct AVM2 implementation, the method prologue does:
			//   getlocal_0; pushscope
			// which pushes 'this' into the scope. We replicate that here.
			int scope_size = env->m_scope.size();
			env->m_scope.push(as_value(this_ptr));

			IF_VERBOSE_ACTION(log_msg("\nEX: call method #%d\n", m_method));

			// Execute the actions.
			execute(local_register, env, fn.result);

			IF_VERBOSE_ACTION(log_msg("EX: ended #%d.\n\n", m_method));

			// Pop 'this' from the scope stack
			while (env->m_scope.size() > scope_size)
			{
				env->m_scope.pop();
			}

			if (stack_size != env->size())
			{
				log_error("error: stack size on exit must be same as on entry, %d:%d \n",
					stack_size, env->size());

				// restore stack size - only shrink, never grow
				if (env->size() > stack_size)
				{
					env->resize(stack_size);
				}
				// If stack is smaller than expected, we can't fix it - just continue
			}
		}

	}

	// interperate action script bytecode
	void	as_3_function::execute(array<as_value>& lregister, as_environment* env, as_value* result)
	{
		// m_abc may be destroyed
		assert(m_abc != NULL);

		vm_stack& stack = *env;
		vm_stack& scope = env->m_scope;

		// some method have no body
		if (m_code.size() == 0)
		{
			return;
		}

		{
			static int s_mlog = 0;
			if (s_mlog < 400)
			{
				const char* nm = "?";
				if (m_abc != NULL)
				{
					const char* n2 = m_abc->get_multiname(m_name);
					if (n2) nm = n2;
				}
				fprintf(stderr, "[AVM2EXEC] enter method=%s m=%d locals=%d params=%d codelen=%d this=%s\n",
					nm ? nm : "?", m_method, m_local_count, (int)m_param_type.size(), m_code.size(),
					lregister[0].to_xstring());
				fflush(stderr);
				s_mlog++;
			}
		}

		const char* cur_meth = "?";
		if (m_abc != NULL)
		{
			const char* n2 = m_abc->get_multiname(m_name);
			if (n2) cur_meth = n2;
		}

		int ip = 0;
		const bool trace_ops = (m_code.size() == 191 && m_local_count == 6);
		do
		{
			Uint8 opcode = m_code[ip];
			if (trace_ops)
			{
				fprintf(stderr, "[OP] ip=%04X op=%02X\n", ip, opcode);
				fflush(stderr);
			}
			ip++;
			switch (opcode)
			{
			case 0x11: // iftrue
				{
					bool taken;
					//Follows ECMA-262 11.9.3
					taken = stack.top(0).to_bool();
					stack.drop(1);
					if (taken)
					{
						int offset = m_code[ip] | m_code[ip+1]<<8 | m_code[ip+2]<<16;
						ip += offset;
					}

					ip += 3;

					IF_VERBOSE_ACTION(log_msg("EX: iftrue\t %s\n", taken? "taken": "not taken"));
				} break;

				case 0x12: // iffalse
				{
					bool taken;
					//Follows ECMA-262 11.9.3
					taken = !stack.top(0).to_bool();
					stack.drop(1);
					if (taken)
					{
						int offset = m_code[ip] | m_code[ip+1]<<8 | m_code[ip+2]<<16;
						ip += offset;
					}

					ip += 3;

					IF_VERBOSE_ACTION(log_msg("EX: iffalse\t %s\n", taken? "taken": "not taken"));
				} break;

				case 0x14:	// ifne
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					if (!as_value::abstract_equality_comparison(a, b))
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifne %d\n", offset));
					break;
				}

				case 0x1D: // popscope
				{
					scope.pop();

					IF_VERBOSE_ACTION(log_msg("EX: popscope\n"));
					break;
				}

				case 0x20:  // pushnull
				{
					as_value value;

					value.set_null();
					stack.push( value );
					IF_VERBOSE_ACTION(log_msg("EX: pushnull\n"));

				} break;

				case 0x24:	// pushbyte
				{
					int byte_value = (int8_t)m_code[ip++];
					stack.push(byte_value);
					IF_VERBOSE_ACTION(log_msg("EX: pushbyte\t %d\n", byte_value));
					break;
				}

				case 0x25:  // pushshort
				{
					int val;
					ip += read_vu30(val, &m_code[ip]);
					stack.push(val);
					IF_VERBOSE_ACTION(log_msg("EX: pushshort\t %d\n", val));
					break;
				}

				case 0x26:  // pushtrue
				{
					stack.push( true );

					IF_VERBOSE_ACTION(log_msg("EX: pushtrue\n"));
				}
				break;

				case 0x27:  // pushfalse
				{
					stack.push( false );

					IF_VERBOSE_ACTION(log_msg("EX: pushfalse\n"));
				}
				break;

				case 0x29:  // pop the value from stack and discard it
				{
					stack.pop();
					IF_VERBOSE_ACTION(log_msg("EX: pop\n"));
					break;
				}

				case 0x2A:  // dup
				{
					IF_VERBOSE_ACTION(log_msg("EX: dup %s\n", stack.top(0).to_xstring()));
					stack.push(stack.top(0));
				} break;

				case 0x2D:	// pushint
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					int val = m_abc->get_integer(index);
					stack.push(val);

					IF_VERBOSE_ACTION(log_msg("EX: pushint\t %d\n", val));

					break;
				}

				case 0x2C:	// pushstring
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					const char* val = m_abc->get_string(index);
					stack.push(val);

					IF_VERBOSE_ACTION(log_msg("EX: pushstring\t '%s'\n", val));

					break;
				}

				case 0x2F:	// pushdouble
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					double val = m_abc->get_double(index);
					stack.push(val);

					IF_VERBOSE_ACTION(log_msg("EX: pushdouble\t %f\n", val));

					break;
				}

				case 0x30:	// pushscope
				{
					as_value val = stack.pop();
					scope.push(val);

					IF_VERBOSE_ACTION(log_msg("EX: pushscope\t %s\n", val.to_xstring()));

					break;
				}

				case 0x46:  // callproperty
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					const char* name = m_abc->get_multiname(index);

					int arg_count;
					ip += read_vu30(arg_count, &m_code[ip]);

					// Handle AS3 EventDispatcher.dispatchEvent(event)
					if (strcmp(name, "dispatchEvent") == 0 && arg_count >= 1)
					{
						as_object* target = stack.top(arg_count).to_object();
						as_value evt = stack.top(0);
						avm2_dispatch_event(target, evt, env);
						stack.drop(arg_count + 1);
						stack.push(as_value(true));
						IF_VERBOSE_ACTION(log_msg("EX: callproperty\t dispatchEvent\n"));
						break;
					}

					// Handle AVM2 display list methods (read args directly from stack)
					// Stack layout: ..., obj, arg1, ..., argN
					// stack.top(arg_count) = obj (receiver), stack.top(arg_count-1) = arg1, ..., stack.top(0) = argN
					{
					sprite_instance* target_sprite = cast_to<sprite_instance>(stack.top(arg_count).to_object());
					if (target_sprite && strcmp(name, "addChild") == 0 && arg_count >= 1)
					{
						character* child = stack.top(0).to_object() ? cast_to<character>(stack.top(0).to_object()) : NULL;
						character* result_ch = target_sprite->avm2_add_child(child);
						stack.drop(arg_count + 1);
						stack.push(as_value(result_ch));
						break;
					}
					if (target_sprite && strcmp(name, "addChildAt") == 0 && arg_count >= 2)
					{
						character* child = stack.top(1).to_object() ? cast_to<character>(stack.top(1).to_object()) : NULL;
						int idx = (int)stack.top(0).to_number();
						character* result_ch = target_sprite->avm2_add_child_at(child, idx);
						stack.drop(arg_count + 1);
						stack.push(as_value(result_ch));
						break;
					}
					if (target_sprite && strcmp(name, "removeChild") == 0 && arg_count >= 1)
					{
						character* child = stack.top(0).to_object() ? cast_to<character>(stack.top(0).to_object()) : NULL;
						character* result_ch = target_sprite->avm2_remove_child(child);
						stack.drop(arg_count + 1);
						stack.push(as_value(result_ch));
						break;
					}
					if (target_sprite && strcmp(name, "removeChildAt") == 0 && arg_count >= 1)
					{
						int idx = (int)stack.top(0).to_number();
						character* result_ch = target_sprite->avm2_remove_child_at(idx);
						stack.drop(arg_count + 1);
						stack.push(as_value(result_ch));
						break;
					}
					if (target_sprite && strcmp(name, "getChildAt") == 0 && arg_count >= 1)
					{
						int idx = (int)stack.top(0).to_number();
						character* result_ch = target_sprite->avm2_get_child_at(idx);
						stack.drop(arg_count + 1);
						stack.push(as_value(result_ch));
						break;
					}
					if (target_sprite && strcmp(name, "getChildIndex") == 0 && arg_count >= 1)
					{
						character* child = stack.top(0).to_object() ? cast_to<character>(stack.top(0).to_object()) : NULL;
						int idx = target_sprite->avm2_get_child_index(child);
						stack.drop(arg_count + 1);
						stack.push(as_value((double)idx));
						break;
					}
					if (target_sprite && strcmp(name, "numChildren") == 0 && arg_count == 0)
					{
						int num = target_sprite->avm2_get_num_children();
						stack.drop(arg_count + 1);
						stack.push(as_value((double)num));
						break;
					}
					if (target_sprite && strcmp(name, "contains") == 0 && arg_count >= 1)
					{
						character* child = stack.top(0).to_object() ? cast_to<character>(stack.top(0).to_object()) : NULL;
						bool contains = target_sprite->avm2_contains(child);
						stack.drop(arg_count + 1);
						stack.push(as_value(contains));
						break;
					}
					if (target_sprite && strcmp(name, "setChildIndex") == 0 && arg_count >= 2)
					{
						character* child = stack.top(1).to_object() ? cast_to<character>(stack.top(1).to_object()) : NULL;
						int idx = (int)stack.top(0).to_number();
						target_sprite->avm2_set_child_index(child, idx);
						stack.drop(arg_count + 1);
						stack.push(as_value());
						break;
					}
					if (target_sprite && strcmp(name, "swapChildren") == 0 && arg_count >= 2)
					{
						character* child1 = stack.top(1).to_object() ? cast_to<character>(stack.top(1).to_object()) : NULL;
						character* child2 = stack.top(0).to_object() ? cast_to<character>(stack.top(0).to_object()) : NULL;
						target_sprite->avm2_swap_children(child1, child2);
						stack.drop(arg_count + 1);
						stack.push(as_value());
						break;
					}
					if (target_sprite && strcmp(name, "swapChildrenAt") == 0 && arg_count >= 2)
					{
						int idx1 = (int)stack.top(1).to_number();
						int idx2 = (int)stack.top(0).to_number();
						target_sprite->avm2_swap_children_at(idx1, idx2);
						stack.drop(arg_count + 1);
						stack.push(as_value());
						break;
					}
					}

					as_environment env(get_player());
					for (int i = 0; i < arg_count; i++)
					{
						env.push(stack.top(i));
					}
					stack.drop(arg_count);

					as_value result;

					if( stack.top(0).is_object() )
					{
						as_object* obj = stack.top(0).to_object();

						as_value func, func2;

						result.set_undefined();

						if( obj &&  obj->get_member(name, &func))
						{
							if( func.is_function() )
							{
								result = call_method(func, &env, obj, arg_count, env.get_top_index()); 
							}
							else if(func.to_object() && func.to_object()->get_member( "__call__", &func2 ) )
							{
								//todo patch scope
								result = call_method(func2, &env, obj, arg_count, env.get_top_index());
							}
						else
						{
							// AS3 coercion call, e.g. MovieClip(x), String(x), int(x).
							// The callee is a class object rather than a callable, so
							// convert (or pass through) the single argument.
							if (arg_count == 1 && func.to_object() != NULL)
							{
								as_value arg = env.top(0);
								if (strcmp(name, "int") == 0)
								{
									arg.set_double((double)(int)arg.to_number());
								}
								else if (strcmp(name, "uint") == 0)
								{
									arg.set_double((double)(Uint32)arg.to_number());
								}
								else if (strcmp(name, "Number") == 0)
								{
									arg.set_double(arg.to_number());
								}
								else if (strcmp(name, "String") == 0)
								{
									arg.set_string(arg.to_string());
								}
								else if (strcmp(name, "Boolean") == 0)
								{
									arg.set_bool(arg.to_bool());
								}
								result = arg;
							}
							else
							{
								fprintf(stderr, "[CP_NOTFN] '%s' args=%d obj=%p type=%s meth=%s\n",
									name, arg_count, obj, func.to_xstring(), cur_meth);
								fflush(stderr);
							}
						}
						}
						else
						{
							fprintf(stderr, "[CP_MISS] '%s' args=%d obj=%p meth=%s\n",
								name, arg_count, obj, cur_meth);
							fflush(stderr);
						}
					}
					else
					{
						as_value func;
						if( stack.top(0).find_property( name, &func ) )
						{
							result = call_method(func, &env, stack.top(0), arg_count, env.get_top_index()); 
						}
					}

					IF_VERBOSE_ACTION(log_msg("EX: callproperty\t 0x%p.%s(args:%d), result %s\n", stack.top(0).to_xstring(), name, arg_count, result.to_xstring()));

					{
						static char s_cseen[512][96];
						static int s_ncseen = 0;
						int hit = -1;
						for (int k = 0; k < s_ncseen; k++)
						{
							if (strcmp(s_cseen[k], name) == 0) { hit = k; break; }
						}
						if (hit < 0 && s_ncseen < 512)
						{
							strncpy(s_cseen[s_ncseen], name, 95);
							s_cseen[s_ncseen][95] = 0;
							s_ncseen++;
							fprintf(stderr, "[CP] '%s' args=%d obj=%s result=%s meth=%s\n",
								name, arg_count, stack.top(0).to_xstring(),
								result.to_xstring(), cur_meth);
							fflush(stderr);
						}
					}

					stack.drop(1);
					stack.push( result );
					
				} break;

				case 0x47:	// returnvoid
				{
					IF_VERBOSE_ACTION(log_msg("EX: returnvoid\t\n"));
					result->set_undefined();
					return;
				}

				case 0x48:	// returnvalue
				{
					IF_VERBOSE_ACTION(log_msg("EX: returnvalue \t%s\n", stack.top(0).to_xstring()));
					*result = stack.pop();
					return;
				}

				case 0x49:	// constructsuper
				{
					// stack: object, arg1, arg2, ..., argn
					int arg_count;
					ip += read_vu30(arg_count, &m_code[ip]);

					as_environment env(get_player());
					for (int i = 0; i < arg_count; i++)
					{
						env.push(stack.pop());
					}

					gc_ptr<as_object> obj = stack.pop().to_object();

					// Assume we are in a constructor
					tu_string class_name = m_abc->get_class_from_constructor( m_method );
					tu_string super_class_name = m_abc->get_super_class( class_name );

					as_object * super = obj.get_ptr();

					while( super->get_proto() )
					{
						super = super->get_proto();
					}

					as_function * function = m_abc->get_class_constructor( super_class_name );
					if( !function )
					{
						as_value value;
						if( get_player()->get_global()->get_member( super_class_name, &value ) )
						{
							function = cast_to<as_function>( value.to_object() );
						}
					}
					if( !function )
					{
						// The super class is a gameswf built-in class object, which is not
						// callable.  For AS3 event subclasses apply the base Event
						// constructor semantics directly, otherwise the instance would be
						// left without a "type" and dispatchEvent could not route it.
						if( obj.get_ptr() && arg_count >= 1 &&
							strcmp( super_class_name.c_str(), "Event" ) == 0 )
						{
							as_value cur;
							if( !obj->get_member( "type", &cur ) || cur.is_undefined() )
							{
								obj->set_member( "type", env.top(0) );
								obj->set_member( "bubbles", as_value( arg_count >= 2 ? env.top(1).to_bool() : false ) );
								obj->set_member( "cancelable", as_value( arg_count >= 3 ? env.top(2).to_bool() : false ) );
								obj->set_member( "eventPhase", as_value( 0 ) );
								obj->set_member( "target", as_value() );
								obj->set_member( "currentTarget", as_value() );
							}
						}
						// else: skip the super constructor call
					}
					else
					{
						as_object* proto = super->create_proto( function );
						UNUSED(proto);
						call_method( function, &env, obj.get_ptr(), arg_count, 0);
					}

					//stack.top(0) = obj.get_ptr();

					IF_VERBOSE_ACTION(log_msg("EX: constructsuper\t 0x%p(args:%d)\n", obj.get_ptr(), arg_count));

					break;
				}

				case 0x4A: //constructprop
				// Stack ..., obj, [ns], [name], arg1,...,argn => ..., value
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					const char* name = m_abc->get_multiname(index);
					const char * name_space = m_abc->get_multiname_namespace(index);
					UNUSED(name_space);

					int arg_count;
					ip += read_vu30(arg_count, &m_code[ip]);

					as_environment env(get_player());
					for (int i = 0; i < arg_count; i++)
					{
						env.push(stack.top(i));
					}
					stack.drop(arg_count);

					as_object* obj = stack.pop().to_object();

					as_value func, func2;

					gc_ptr<as_object> new_object;
					// Use find_property instead of get_member to search class traits
					// for the constructor. get_member only searches m_members + proto chain
					// but won't find methods defined in class traits unless already cached.
					bool found = false;
					if (obj)
					{
						found = obj->find_property(name, &func);
					}
					if (strcmp(name, "URLLoader") == 0 || strcmp(name, "URLRequest") == 0)
					{
						fprintf(stderr, "[CPC] '%s' obj=%p found=%d func=%s func_obj=%p\n",
							name, obj, (int)found, func.to_xstring(),
							func.is_object() ? func.to_object() : NULL);
						fflush(stderr);
					}
					if( found )
					{
						// Check if this is a built-in type that should NOT be created as sprite
						bool is_display_object = true;
						
						if (strcmp(name, "Array") == 0)
						{
							is_display_object = false;
							new_object = new as_array(get_player());
						}
						else if (strcmp(name, "Object") == 0)
						{
							is_display_object = false;
							new_object = new as_object(get_player());
						}
						else if (strcmp(name, "String") == 0)
						{
							is_display_object = false;
							// String constructor returns a string value, not an object
							if (arg_count > 0)
							{
								stack.push(env.top(0).to_string());
							}
							else
							{
								stack.push("");
							}
							break;
						}
						else if (strcmp(name, "Number") == 0 || strcmp(name, "int") == 0 || strcmp(name, "uint") == 0)
						{
							is_display_object = false;
							if (arg_count > 0)
							{
								stack.push(env.top(0).to_number());
							}
							else
							{
								stack.push(0.0);
							}
							break;
						}
						else if (strcmp(name, "Boolean") == 0)
						{
							is_display_object = false;
							if (arg_count > 0)
							{
								stack.push(env.top(0).to_bool());
							}
							else
							{
								stack.push(false);
							}
							break;
						}
						else if (strcmp(name, "XML") == 0 || strcmp(name, "XMLList") == 0)
						{
							is_display_object = false;
							new_object = new as_object(get_player());
						}
						else if (strcmp(name, "RegExp") == 0)
						{
							is_display_object = false;
							new_object = new as_object(get_player());
						}
						else if (strcmp(name, "ByteArray") == 0)
						{
							is_display_object = false;
							new_object = new as_object(get_player());
						}
						else if (strcmp(name, "Point") == 0 || strcmp(name, "Rectangle") == 0 
							|| strcmp(name, "Matrix") == 0 || strcmp(name, "ColorTransform") == 0
							|| strcmp(name, "Transform") == 0)
						{
							is_display_object = false;
							new_object = new as_object(get_player());
						}
						else if (strcmp(name, "Event") == 0)
						{
							is_display_object = false;
							new_object = new as_object(get_player());
							if (arg_count > 0)
							{
								new_object->set_member("type", env.top(0));
							}
						}
					else if (strcmp(name, "Timer") == 0 || strcmp(name, "URLRequest") == 0
							|| strcmp(name, "URLLoader") == 0 || strcmp(name, "Sound") == 0
							|| strcmp(name, "SoundChannel") == 0 || strcmp(name, "SoundTransform") == 0)
					{
						is_display_object = false;
						new_object = new as_object(get_player());
						if (strcmp(name, "URLRequest") == 0 && arg_count > 0)
						{
							// new URLRequest(url) -- the constructor special case above
							// has no ABC ctor to run, so store the address directly.
							new_object->set_member("url", env.top(0));
							new_object->set_member("__url__", env.top(0));
						}
					}
						else
						{
							// For custom classes, check the DIRECT super class name
							// to determine if this class extends a display object class.
							// We only check the immediate parent (not the full chain)
							// for performance - the class hierarchy in typical SWFs is:
							//   CustomSprite -> Sprite -> DisplayObjectContainer -> InteractiveObject -> DisplayObject -> Object
							// If the parent is in our known display class list, it's a display object.
							bool extends_display = false;
							tu_string class_name_str(name);
							instance_info* ii = m_abc->get_instance_info(class_name_str);
							if (ii != NULL && ii->m_super_name > 0)
							{
								const char* super_name = m_abc->get_multiname(ii->m_super_name);
								if (super_name)
								{
									fprintf(stderr, "[EXTD2] class='%s' super='%s'\n",
										class_name_str.c_str(), super_name);
									fflush(stderr);
									// Check if parent is a known display class
									// Also check grandparent for deeper custom class chains like:
									// CustomUI extends CustomSprite extends Sprite
									if (strcmp(super_name, "Sprite") == 0 || strcmp(super_name, "MovieClip") == 0 ||
										strcmp(super_name, "SimpleButton") == 0 || strcmp(super_name, "TextField") == 0 ||
										strcmp(super_name, "DisplayObject") == 0 || strcmp(super_name, "DisplayObjectContainer") == 0 ||
										strcmp(super_name, "InteractiveObject") == 0 ||
										strcmp(super_name, "Shape") == 0)
									{
										extends_display = true;
									}
									else
									{
										// Check grandparent (for CustomUI extends CustomSprite extends Sprite)
										tu_string super_name_str(super_name);
										instance_info* super_ii = m_abc->get_instance_info(super_name_str);
										if (super_ii != NULL && super_ii->m_super_name > 0)
										{
											const char* grandparent = m_abc->get_multiname(super_ii->m_super_name);
											if (grandparent && (
												strcmp(grandparent, "Sprite") == 0 || strcmp(grandparent, "MovieClip") == 0 ||
												strcmp(grandparent, "SimpleButton") == 0 || strcmp(grandparent, "TextField") == 0 ||
												strcmp(grandparent, "DisplayObject") == 0 || strcmp(grandparent, "DisplayObjectContainer") == 0 ||
												strcmp(grandparent, "InteractiveObject") == 0 ||
												strcmp(grandparent, "Shape") == 0))
											{
												extends_display = true;
											}
										}
									}
								}
							}
							
						if (extends_display)
						{
							// This custom class extends a display object - create sprite_instance
							// Set is_display_object = false to prevent the subsequent
							// if(is_display_object) from creating a SECOND sprite_instance
							is_display_object = false;
							sprite_definition* empty_def = new sprite_definition(get_player(), NULL);
							sprite_instance* sprite = new sprite_instance(
								get_player(), empty_def,
								lregister[0].to_object() ? lregister[0].to_object()->get_root() : NULL,
								cast_to<sprite_instance>(lregister[0].to_object()),
								0);
							sprite->set_name(name);
							sprite->set_instance(m_abc->get_instance_info(name));
							new_object = sprite;

							// Attach a Graphics object so AS3 drawing calls work
							as_graphics* gfx = new as_graphics(get_player(), sprite);
							sprite->set_member("graphics", as_value(gfx));
						}
							else
							{
								is_display_object = false;
								new_object = new as_object(get_player());
								fprintf(stderr, "[BIRTH-CP] obj=%p name='%s'\n", (void*) new_object.get_ptr(), name ? name : "?");
								fflush(stderr);
								// Set instance info so get_member can search instance traits
								// for methods defined in the AS3 instance body.
								new_object->set_instance(m_abc->get_instance_info(name));
								{
									instance_info* dii = m_abc->get_instance_info(name);
									const char* dsuper = (dii && dii->m_super_name > 0) ? m_abc->get_multiname(dii->m_super_name) : "?";
									fprintf(stderr, "[CPC-PLAIN] name='%s' super='%s' ii=%p\n",
										name ? name : "?", dsuper ? dsuper : "?", (void*) dii);
									fflush(stderr);
								}
							}
						}

						// Set up prototype chain: instance.__proto__ = class object
						// This allows get_member() to find class methods via prototype chain
						// The class object (as_class) overrides get_member to search class traits
						//
						// `obj` is only the object that OWNS the property; for built-ins
						// registered on _global (URLLoader, URLRequest, ...) that is
						// _global itself.  Resolve the property value so the real class
						// object becomes the prototype.
						{
							as_value class_val;
							as_object* cls_obj = NULL;
							if (obj && obj->get_member(name, &class_val) && class_val.is_object())
							{
								cls_obj = class_val.to_object();
							}
							if (cls_obj != NULL && cast_to<as_class>(cls_obj) == NULL)
							{
								// Plain built-in class object: make it the prototype so
								// its members are reachable, but keep _global reachable
								// through it when it has no prototype of its own.
								if (cls_obj->get_proto() == NULL)
								{
									cls_obj->m_proto = obj;
								}
								new_object->m_proto = cls_obj;
							}
							else if (obj)
							{
								new_object->m_proto = obj;
							}
						}

					if (is_display_object)
					{
						// Create a sprite_instance for display objects
						sprite_definition* empty_def = new sprite_definition(get_player(), NULL);
						sprite_instance* sprite = new sprite_instance(
							get_player(), empty_def, 
							lregister[0].to_object() ? lregister[0].to_object()->get_root() : NULL,
							cast_to<sprite_instance>(lregister[0].to_object()),
							0);
						sprite->set_name(name);

						new_object = sprite;
						new_object->set_instance( m_abc->get_instance_info( name ) );

						// Attach a Graphics object so AS3 drawing calls work
						as_graphics* gfx = new as_graphics(get_player(), sprite);
						sprite->set_member("graphics", as_value(gfx));
					}
						else
						{
							// For non-display objects, also set instance_info so
							// find_property can search instance traits (slots etc.)
							new_object->set_instance( m_abc->get_instance_info( name ) );
						}

						// Call the constructor if we have one
						as_function* ctor = m_abc->get_class_constructor(name);
						if (ctor)
						{
							call_method(ctor, &env, new_object.get_ptr(), arg_count, 0);
						}
					}
					IF_VERBOSE_ACTION(log_msg("EX: constructprop\t 0x%p.%s(args:%d)\n", obj, name, arg_count));

					stack.push( new_object.get_ptr() );
				}
				break;

				case 0x4F:	// callpropvoid, Call a property, discarding the return value.
				// Stack: ..., obj, [ns], [name], arg1,...,argn => ...
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					const char* name = m_abc->get_multiname(index);

					int arg_count;
					ip += read_vu30(arg_count, &m_code[ip]);

					// Handle trace() built-in function
					if (strcmp(name, "trace") == 0)
					{
						// Stack: ..., receiver, arg1, ..., argN
						// stack.top(arg_count) = receiver, stack.top(arg_count-i) = argi
						tu_string trace_str;
						for (int i = 1; i <= arg_count; i++)
						{
							if (i > 1) trace_str += " ";
							trace_str += stack.top(arg_count - i).to_string();
						}
						stack.drop(arg_count + 1);
						fprintf(stderr, "[TRACE] %s\n", trace_str.c_str());
						IF_VERBOSE_ACTION(log_msg("EX: callpropvoid\t trace(args:%d)\n", arg_count));
						break;
					}

					// Handle AS3 EventDispatcher.dispatchEvent(event)
					if (strcmp(name, "dispatchEvent") == 0 && arg_count >= 1)
					{
						as_object* target = stack.top(arg_count).to_object();
						as_value evt = stack.top(0);
						avm2_dispatch_event(target, evt, env);
						stack.drop(arg_count + 1);
						IF_VERBOSE_ACTION(log_msg("EX: callpropvoid\t dispatchEvent\n"));
						break;
					}

					// Handle addEventListener (standard Flash event registration)
					if (strcmp(name, "addEventListener") == 0 && arg_count >= 2)
					{
						// Stack: ..., obj, arg1, arg2 => ...
						as_object* target = stack.top(arg_count).to_object();
						as_value event_type_val = stack.top(arg_count - 1);  // first arg = event type
						as_value listener = stack.top(arg_count - 2);    // second arg = listener
						
						// Resolve event type: if it's an object, try to get .type property
						tu_string event_type_str;
						if (event_type_val.is_object() && event_type_val.to_object() != NULL)
						{
							as_object* event_obj = event_type_val.to_object();
							as_value type_val;
							if (event_obj->get_member("type", &type_val) && !type_val.is_undefined())
							{
								event_type_str = type_val.to_string();
							}
							else
							{
								event_type_str = event_type_val.to_string();
							}
						}
						else
						{
							event_type_str = event_type_val.to_string();
						}
						
						if (target)
						{
							tu_string event_key = "__events_";
							event_key += event_type_str;
							
							// Get or create the listeners array for this event type
							as_value listeners_val;
							as_array* listeners_array = NULL;
							if (target->get_member(event_key, &listeners_val) && listeners_val.is_object())
							{
								listeners_array = cast_to<as_array>(listeners_val.to_object());
							}
							
							if (listeners_array == NULL)
							{
								// Create new array for this event type
								listeners_array = new as_array(get_player());
								target->set_member(event_key, as_value(listeners_array));
							}

						// Flash ignores a repeated registration of the same
						// listener for the same event type.  Frame scripts and
						// retry/error handlers re-register on every cycle, so
						// without this guard the array grows without bound and
						// each dispatch re-runs every stale copy.
						as_object* listener_obj = listener.to_object();
						bool duplicate = false;
						if (listener_obj != NULL)
						{
							for (int i = 0; i < listeners_array->size(); i++)
							{
								if (listeners_array->m_array[i].to_object() == listener_obj)
								{
									duplicate = true;
									break;
								}
							}
						}
						if (duplicate)
						{
							stack.drop(arg_count + 1);
							break;
						}
						listeners_array->push(listener);
						fprintf(stderr, "[ADDLIS] target=%p type='%s' key='%s' listener_isfn=%d count=%d fn=%p sprite=%d\n",
							target, event_type_str.c_str(), event_key.c_str(),
							listener.is_function() ? 1 : 0, listeners_array->size(),
							listener.is_function() ? (void*)listener.to_function() : NULL,
							cast_to<sprite_instance>(target) != NULL ? 1 : 0);
					}
					stack.drop(arg_count + 1);
						break;
					}

					// Handle removeEventListener
					if (strcmp(name, "removeEventListener") == 0 && arg_count >= 2)
					{
						as_object* target = stack.top(arg_count).to_object();
						bool ok = avm2_remove_event_listener(target,
							stack.top(arg_count - 1), stack.top(arg_count - 2));
						fprintf(stderr, "[REMLIS] target=%011p ok=%d\n",
							target, ok ? 1 : 0);
						stack.drop(arg_count + 1);
						break;
					}

					// NOTE: 'add', 'dispatch', 'frameHandler' etc. are custom AS3 class methods.
					// They should be resolved via prototype chain (as_class::find_property).
					// No special interception needed - let them fall through to the generic handler.

				// Handle AVM2 display list methods (void variants)
				// Stack layout: ..., obj, arg1, arg2, ..., argN
				// stack.top(arg_count) = obj (receiver), stack.top(arg_count-1) = arg1, ..., stack.top(0) = argN
				{
				// Diagnostic: trace all addChild/addChildAt/removeChild calls
				if (strcmp(name, "addChild") == 0 || strcmp(name, "addChildAt") == 0 ||
					strcmp(name, "removeChild") == 0 || strcmp(name, "removeChildAt") == 0 ||
					strcmp(name, "numChildren") == 0 || strcmp(name, "getChildAt") == 0 ||
					strcmp(name, "getChildIndex") == 0 || strcmp(name, "contains") == 0)
				{
					as_object* dbg_obj = stack.top(arg_count).to_object();
					fprintf(stderr, "[DIAG_CALLPROP] %s arg_count=%d obj=%p obj_type=%s\n",
						name, arg_count, dbg_obj, dbg_obj ? dbg_obj->to_string() : "null");
				}
				sprite_instance* target_sprite = cast_to<sprite_instance>(stack.top(arg_count).to_object());
				if (target_sprite && strcmp(name, "addChild") == 0 && arg_count >= 1)
					{
						character* child = stack.top(0).to_object() ? cast_to<character>(stack.top(0).to_object()) : NULL;
						target_sprite->avm2_add_child(child);
						stack.drop(arg_count + 1);
						break;
					}
					if (target_sprite && strcmp(name, "addChildAt") == 0 && arg_count >= 2)
					{
						character* child = stack.top(1).to_object() ? cast_to<character>(stack.top(1).to_object()) : NULL;
						int idx = (int)stack.top(0).to_number();
						target_sprite->avm2_add_child_at(child, idx);
						stack.drop(arg_count + 1);
						break;
					}
					if (target_sprite && strcmp(name, "removeChild") == 0 && arg_count >= 1)
					{
						character* child = stack.top(0).to_object() ? cast_to<character>(stack.top(0).to_object()) : NULL;
						target_sprite->avm2_remove_child(child);
						stack.drop(arg_count + 1);
						break;
					}
					if (target_sprite && strcmp(name, "removeChildAt") == 0 && arg_count >= 1)
					{
						int idx = (int)stack.top(0).to_number();
						target_sprite->avm2_remove_child_at(idx);
						stack.drop(arg_count + 1);
						break;
					}
					if (target_sprite && strcmp(name, "setChildIndex") == 0 && arg_count >= 2)
					{
						character* child = stack.top(1).to_object() ? cast_to<character>(stack.top(1).to_object()) : NULL;
						int idx = (int)stack.top(0).to_number();
						target_sprite->avm2_set_child_index(child, idx);
						stack.drop(arg_count + 1);
						break;
					}
					if (target_sprite && strcmp(name, "swapChildren") == 0 && arg_count >= 2)
					{
						character* child1 = stack.top(1).to_object() ? cast_to<character>(stack.top(1).to_object()) : NULL;
						character* child2 = stack.top(0).to_object() ? cast_to<character>(stack.top(0).to_object()) : NULL;
						target_sprite->avm2_swap_children(child1, child2);
						stack.drop(arg_count + 1);
						break;
					}
					if (target_sprite && strcmp(name, "swapChildrenAt") == 0 && arg_count >= 2)
					{
						int idx1 = (int)stack.top(1).to_number();
						int idx2 = (int)stack.top(0).to_number();
						target_sprite->avm2_swap_children_at(idx1, idx2);
						stack.drop(arg_count + 1);
						break;
					}
					}

					as_environment env(get_player());
					for (int i = 0; i < arg_count; i++)
					{
						env.push(stack.top(i));
					}
					stack.drop(arg_count);

					as_value recv_val = stack.top(0);
					as_object* obj = stack.pop().to_object();

					as_value func, func2;

					bool got = obj ? obj->get_member(name, &func) : false;

					if (strcmp(name, "dispatchEvent") == 0)
					{
						as_object* p = obj ? obj->get_proto() : NULL;
						as_value pl;
						int pload = p ? (int)p->get_member(name, &pl) : -1;
						fprintf(stderr, "[DPDBG] '%s' obj=%p got=%d isfn=%d func=%s proto=%p proto_has=%d proto_val=%s arg_count=%d\n",
							name, obj, (int)got, (int)func.is_function(), func.to_xstring(),
							p, pload, pl.to_xstring(), arg_count);
						fflush(stderr);
					}

					if (strcmp(name, "load") == 0 || strcmp(name, "close") == 0)
					{
						as_object* p = obj ? obj->get_proto() : NULL;
						as_value pl;
						int pload = p ? (int)p->get_member(name, &pl) : -1;
						fprintf(stderr, "[LOADDBG] '%s' obj=%p got=%d isfn=%d func=%s proto=%p proto2=%p proto_has=%d proto_val=%s\n",
							name, obj, (int)got, (int)func.is_function(), func.to_xstring(),
							p, p ? p->get_proto() : NULL, pload, pl.to_xstring());
						fflush(stderr);
					}

					if( got )
					{
						if( func.is_function() )
						{
							call_method(func, &env, obj, arg_count, env.get_top_index());
						}
						else if(func.to_object() && func.to_object()->get_member( "__call__", &func2 ) )
						{
							//todo patch scope
							call_method(func2, &env, obj, arg_count, env.get_top_index());
						}
					else
					{
						IF_VERBOSE_ACTION(log_error("CALLPROPVOID: %s.%s found but not callable (type=%s)\n",
							obj->to_string(), name, func.to_xstring()));
					}
					}
					else
					{
						IF_VERBOSE_ACTION(log_error("CALLPROPVOID: %s.%s not found\n",
							obj ? obj->to_string() : "null", name));
					}

					{
						static char s_seen[1024][96];
						static int s_nseen = 0;
						int hit = -1;
						for (int k = 0; k < s_nseen; k++)
						{
							if (strcmp(s_seen[k], name) == 0) { hit = k; break; }
						}
						if (hit < 0 && s_nseen < 1024)
						{
							strncpy(s_seen[s_nseen], name, 95);
							s_seen[s_nseen][95] = 0;
							hit = s_nseen++;
							fprintf(stderr, "[CPV] new method '%s' args=%d found=%d obj=%p\n",
								name, arg_count, obj ? 1 : 0, obj);
							fflush(stderr);
						}
					}

					if (obj == NULL)
					{
						static bool s_dumped_null = false;
						fprintf(stderr, "[CPV_NULL] '%s' args=%d meth=%s ip=%d codelen=%d locals=%d\n",
							name, arg_count, cur_meth, ip, (int) m_code.size(), m_local_count);
						if (s_dumped_null == false)
						{
							s_dumped_null = true;
							fprintf(stderr, "  code:");
							for (int k = 0; k < (int) m_code.size(); k++)
							{
								if ((k % 32) == 0)
								{
									fprintf(stderr, "\n   %04X:", k);
								}
								fprintf(stderr, " %02X", m_code[k]);
							}
							fprintf(stderr, "\n");
						}
						fprintf(stderr, "| recv=%s local0=%s scope_top=%s\n",
							recv_val.to_xstring(),
							lregister[0].to_xstring(),
							scope.size() > 0 ? scope.top(0).to_xstring() : "none");
						fflush(stderr);
					}

					IF_VERBOSE_ACTION(log_msg("EX: callpropvoid\t 0x%p.%s(args:%d)\n", obj, name, arg_count));

					break;
				}

				case 0x56: //newarray
				{
					int arg_count;
					
					ip += read_vu30(arg_count, &m_code[ip]);

					as_array * array = new as_array( get_player() );

					int offset = stack.size() - arg_count;

					for( int arg_index = 0; arg_index < arg_count; ++arg_index )
					{
						array->push( stack[ offset + arg_index ] );
					}

					stack.resize( offset + 1 );
					stack.top(0) = array;

					IF_VERBOSE_ACTION(log_msg("EX: newarray\t arg_count:%i\n", arg_count));

				} break;

				case 0x58: // newclass
				{
					// stack:	..., basetype => ..., newclass
					int class_index;
					ip += read_vu30( class_index, &m_code[ip] );

					// Get instance_info by index (not by name)
					instance_info* ii = m_abc->get_instance_info_by_index(class_index);
					const char* class_name = ii ? m_abc->get_multiname(ii->m_name) : NULL;

					//					as_object* basetype = stack.top(0).to_object();

					gc_ptr<as_class> new_class = new as_class(get_player());

					{
						// Stack top holds the superclass (basetype) until we overwrite
						// it with the new class below.  Wiring it up as this class
						// object's prototype makes inherited members (EventDispatcher.
						// dispatchEvent/addEventListener/...) reachable from instances:
						// instance -> class -> super class -> ... -> _global.
						as_object* basetype = stack.top(0).to_object();
						if (basetype != NULL && basetype != new_class.get_ptr())
						{
							new_class->m_proto = basetype;
						}
					}
					new_class->set_class(m_abc->get_class_info(class_index), class_index);
					// The class object doubles as the prototype for instances of this
					// class, so it also has to expose the class's *instance* traits.
					if (ii != NULL)
					{
						new_class->set_instance(ii);
					}

					{
						class_info* dci = m_abc->get_class_info(class_index);
						fprintf(stderr, "[NEWCLASS] '%s' idx=%d static_traits=%d cinit=%d\n",
							class_name ? class_name : "?", class_index,
							dci ? (int)dci->m_trait.size() : -1,
							dci ? dci->m_cinit : -1);
						fflush(stderr);
					}

					as_environment env( get_player() );
					call_method( m_abc->get_class_function( class_index ), &env, new_class.get_ptr(), 0, 0 );

				// Register the class in the global namespace so constructprop can find it
				if (class_name)
				{
					get_global()->set_member(class_name, as_value(new_class.get_ptr()));
				}

					stack.top(0).set_as_object(new_class.get_ptr());
	
					break;
				}

				case 0x5D:	// findpropstrict
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					const char* name = m_abc->get_multiname(index);

					// search property in scope
					as_object* obj = scope.find_property(name);

					if (strcmp(name, "dispatchEvent") == 0)
					{
						fprintf(stderr, "[FSTRICT] 'dispatchEvent' obj=%p scopeSize=%d\n",
							(void*)obj, scope.size());
						fflush(stderr);
					}

					// AVM2 lazy script init: the first reference to a script trait
					// runs that script's init method, which defines its classes.
					if (obj == NULL)
					{
						script_info* si = m_abc->get_pending_script_by_trait(name);
						if (si != NULL)
						{
							si->m_executed = true;
							as_environment env( get_player() );
							call_method( m_abc->get_method(si->m_init), &env, get_global(), 0, 0 );

							obj = scope.find_property(name);
							if (obj == NULL)
							{
								as_value dummy;
								if (get_global()->get_member(name, &dummy))
								{
									obj = get_global();
								}
							}
							IF_VERBOSE_ACTION(log_msg("EX: findpropstrict\t ran script init for %s, obj=0x%p\n", name, obj));
						}
					}

					//Search for a script entry to execute
					if (obj == NULL)
					{
						as_function* func = m_abc->get_script_function(name);
						if (func != NULL)
						{
							get_global()->set_member( name, new as_object( get_player() ) );

							as_environment env( get_player() );

							call_method( func, &env, get_global(), 0, 0 );

							obj = get_global();
						}
					}
					IF_VERBOSE_ACTION(log_msg("EX: findpropstrict\t %s, obj=0x%p\n", name, obj));

					if (obj == NULL)
					{
						static char s_fpseen[512][96];
						static int s_nfpseen = 0;
						int hit = -1;
						for (int k = 0; k < s_nfpseen; k++)
						{
							if (strcmp(s_fpseen[k], name) == 0) { hit = k; break; }
						}
						if (hit < 0 && s_nfpseen < 512)
						{
							strncpy(s_fpseen[s_nfpseen], name, 95);
							s_fpseen[s_nfpseen][95] = 0;
							s_nfpseen++;
							fprintf(stderr, "[FPROP] '%s' -> NULL meth=%s scopesize=%d top=%s\n",
								name, cur_meth, scope.size(),
								scope.size() > 0 ? scope.top(0).to_xstring() : "none");
							for (int s = 0; s < scope.size(); s++)
							{
								as_object* so = scope[s].to_object();
								as_value probe;
								bool has = so != NULL && so->get_member(name, &probe);
								as_value builtin_probe;
								bool has_builtin = so != NULL && get_builtin(BUILTIN_SPRITE_METHOD, name, &builtin_probe);
								sprite_instance* sp = so ? cast_to<sprite_instance>(so) : NULL;
								fprintf(stderr, "    scope[%d]=%p get_member=%d isfn=%d is_sprite=%d builtin=%d\n",
									s, (void*) so, (int) has, has ? (int) probe.is_function() : 0,
									sp ? 1 : 0, (int) has_builtin);
							}
							fflush(stderr);
						}

						// The scope stack does not always carry the receiver
						// (e.g. an unqualified stop() inside a frame script),
						// and pushing NULL here makes the following call lose
						// its receiver entirely -- the call then silently does
						// nothing.  Fall back to 'this' when it owns the
						// property, then to the global object, which is what
						// findproperty (0x5E) already does.
						as_object* self = lregister.size() > 0 ? lregister[0].to_object() : NULL;
						if (self != NULL)
						{
							as_value probe;
							if (self->get_member(name, &probe))
							{
								obj = self;
							}
						}
						if (obj == NULL)
						{
							as_value probe;
							if (get_global()->get_member(name, &probe))
							{
								obj = get_global();
							}
						}
					}

					stack.push(obj);
					break;
				}

				case 0x5E:	// findproperty, Search the scope stack for a property
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					const char* name = m_abc->get_multiname(index);
					const char * name_space = m_abc->get_multiname_namespace(index);
					UNUSED(name_space);

					as_object* obj = scope.find_property(name);

					if( obj )
					{
						IF_VERBOSE_ACTION(log_msg("EX: findproperty\t '%s', obj=0x%p\n", name, obj));
						stack.push(obj);
					}
					else
					{
						IF_VERBOSE_ACTION(log_msg("EX: findproperty\t '%s', obj=global\n", name));
						stack.push(get_global());
					}
					break;

				}

				case 0x5F:	// finddef
			{
				int index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				const char* name = m_abc->get_multiname(index);
				// Search in scope stack, then global
				as_value val;
				as_object* found_obj = scope.find_property(name);
				if (found_obj)
				{
					found_obj->get_member(name, &val);
				}
				else
				{
					as_object* global = get_global();
					if (global) global->get_member(name, &val);
				}
				stack.push(val);
				IF_VERBOSE_ACTION(log_msg("EX: finddef %s\n", name));
				break;
			}

				case 0x60:	// getlex, Find and get a property.
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					const char* name = m_abc->get_multiname(index);

					// search and get property in scope
					as_value val;
					scope.get_property(name, &val);

					if(val.is_undefined())
					{
						// Check for built-in functions first
						if (strcmp(name, "trace") == 0)
						{
							// Create a trace function object
							gc_ptr<as_object> trace_obj = new as_object(get_player());
							trace_obj->set_member("trace", as_value(trace_obj.get())); // Self-reference for call
							val.set_as_object(trace_obj);
							IF_VERBOSE_ACTION(log_msg("EX: getlex\t trace (builtin)\n"));
						}
						else
						{
							// AVM2 lazy script init: the first reference to a script trait
							// runs that script's init method, which defines its classes
							// and package variables on the global object.
							script_info* si = m_abc->get_pending_script_by_trait(name);
							if (si != NULL)
							{
								si->m_executed = true;
								as_environment env( get_player() );
								call_method( m_abc->get_method(si->m_init), &env, get_global(), 0, 0 );

								// Re-resolve now that the script has defined its symbols.
								val.set_undefined();
								scope.get_property(name, &val);
								if (val.is_undefined())
								{
									get_global()->get_member(name, &val);
								}
								IF_VERBOSE_ACTION(log_msg("EX: getlex\t ran script init for %s, value=%s\n", name, val.to_xstring()));
							}
						}
					}

					if(val.is_undefined())
					{
						as_function* func = m_abc->get_script_function(name);
						if (func != NULL)
						{
								gc_ptr<as_object> object = new as_object( get_player() );
								get_global()->set_member(name, object.get());

								as_environment env( get_player() );

								call_method( func, &env, get_global(), 0, 0 );

								val.set_as_object(object);
						}
					}

					if(val.is_undefined())
					{
						static char s_lseen[512][96];
						static int s_nlseen = 0;
						int hit = -1;
						for (int k = 0; k < s_nlseen; k++)
						{
							if (strcmp(s_lseen[k], name) == 0) { hit = k; break; }
						}
						if (hit < 0 && s_nlseen < 512)
						{
							strncpy(s_lseen[s_nlseen], name, 95);
							s_lseen[s_nlseen][95] = 0;
							s_nlseen++;
							fprintf(stderr, "[GETLEX] '%s' -> undefined meth=%s\n", name, cur_meth);
							fflush(stderr);
						}
					}

					{
						static char s_lxseen[512][96];
						static int s_nlxseen = 0;
						int hit = -1;
						for (int k = 0; k < s_nlxseen; k++)
						{
							if (strcmp(s_lxseen[k], name) == 0) { hit = k; break; }
						}
						if (hit < 0 && s_nlxseen < 512)
						{
							strncpy(s_lxseen[s_nlxseen], name, 95);
							s_lxseen[s_nlxseen][95] = 0;
							s_nlxseen++;
							fprintf(stderr, "[GETLEX] '%s' -> %s meth=%s\n",
								name, val.to_xstring(), cur_meth);
							fflush(stderr);
						}
					}

					IF_VERBOSE_ACTION(log_msg("EX: getlex\t %s, value=%s\n", name, val.to_xstring()));

					stack.push(val);
					break;
				}

				case 0x61: // setproperty
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					
					// Handle late-bound multiname types
					multiname::kind kind = (multiname::kind)m_abc->get_multiname_type(index);
					tu_string name;
					
					switch (kind)
					{
					case multiname::CONSTANT_MultinameL:
					case multiname::CONSTANT_MultinameLA:
					case multiname::CONSTANT_RTQNameL:
					case multiname::CONSTANT_RTQNameLA:
						// Name comes from stack - need to reorder
						// Stack: ..., object, value, name
						{
							as_value name_val = stack.pop();
							as_value value = stack.pop();
							as_value obj_val = stack.pop();
							
							name = name_val.to_string();
							as_object* obj = obj_val.to_object();
							
							IF_VERBOSE_ACTION(log_msg("EX: setproperty\t %s.%s = %s (late-bound)\n", obj_val.to_xstring(), name.c_str(), value.to_xstring()));
							
							if (obj)
							{
								obj->set_member(name, value);
							}
						}
						break;
						
					case multiname::CONSTANT_RTQName:
					case multiname::CONSTANT_RTQNameA:
						// Stack: ..., object, value, ns
						{
							stack.pop();  // discard namespace
							as_value value = stack.pop();
							as_value obj_val = stack.pop();
							
							name = m_abc->get_multiname(index);
							as_object* obj = obj_val.to_object();
							
							IF_VERBOSE_ACTION(log_msg("EX: setproperty\t %s.%s = %s (rtqname)\n", obj_val.to_xstring(), name.c_str(), value.to_xstring()));
							
							if (obj)
							{
								obj->set_member(name, value);
							}
						}
						break;
						
					default:
						// Standard case: name from pool
						{
							name = m_abc->get_multiname(index);

							as_object* obj = stack.top(1).to_object();
							if (obj)
							{
								{
									static char s_pseen[512][96];
									static int s_npseen = 0;
									int hit = -1;
									for (int k = 0; k < s_npseen; k++)
									{
										if (strcmp(s_pseen[k], name.c_str()) == 0) { hit = k; break; }
									}
									if (hit < 0 && s_npseen < 512)
									{
										strncpy(s_pseen[s_npseen], name.c_str(), 95);
										s_pseen[s_npseen][95] = 0;
										s_npseen++;
										fprintf(stderr, "[SETPROP] '%s' = %s obj=%p\n",
											name.c_str(), stack.top(0).to_xstring(), obj);
										fflush(stderr);
									}
								}
								if (name == "visible" || name == "alpha" || name == "mask" || name == "x" || name == "y")
								{
									character* ch = cast_to<character>(obj);
									fprintf(stderr, "[SETPROP2] '%s' = %s obj=%p ch=%p id=%d name=%s\n",
										name.c_str(), stack.top(0).to_xstring(), obj, ch,
										ch ? ch->get_id() : -1,
										ch ? ch->get_name().c_str() : "-");
									fflush(stderr);
								}
								obj->set_member(name, stack.top(0));
							}

							stack.drop(2);
						}
						break;
					}
					
				} break;

				case 0x62: // getlocal
					{
						int index;
						ip += read_vu30(index, &m_code[ip]);

						IF_VERBOSE_ACTION(log_msg("EX: getlocal\t index=%i, value=%s\n", index, lregister[index].to_xstring()));

						stack.push(lregister[index]);

					} break;

				case 0x63: // setlocal
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);

					IF_VERBOSE_ACTION(log_msg("EX: setlocal\t index=%i, value=%s\n", index, stack.top(0).to_xstring()));

					lregister[index] = stack.pop();

				} break;

				case 0x65: // getscopeobject
				{
					int index = m_code[ip];
					++ip;

					assert( index < scope.size() );

					stack.push( scope[index] );

					IF_VERBOSE_ACTION(log_msg("EX: getscopeobject\t index=%i, value=%s\n", index, stack.top(0).to_xstring()));

				} break;

				case 0x66:	// getproperty
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					
					// Handle late-bound multiname types
					multiname::kind kind = (multiname::kind)m_abc->get_multiname_type(index);
					tu_string name;
					as_object* obj = NULL;
					
					switch (kind)
					{
					case multiname::CONSTANT_MultinameL:
					case multiname::CONSTANT_MultinameLA:
					case multiname::CONSTANT_RTQNameL:
					case multiname::CONSTANT_RTQNameLA:
						// Name comes from stack
						// Stack: ..., object, name
						{
							as_value name_val = stack.pop();
							as_value obj_val = stack.pop();
							
							name = name_val.to_string();
							obj = obj_val.to_object();
							
							if (obj)
							{
								as_value result;
								obj->get_member(name, &result);
								stack.push(result);
							}
							else
							{
								stack.push(as_value());  // undefined
							}
							
							IF_VERBOSE_ACTION(log_msg("EX: getproperty\t %s.%s = %s (late-bound)\n", obj_val.to_xstring(), name.c_str(), stack.top(0).to_xstring()));
						}
						break;
						
					case multiname::CONSTANT_RTQName:
					case multiname::CONSTANT_RTQNameA:
						// Stack: ..., object, ns
						{
							stack.pop();  // discard namespace
							as_value obj_val = stack.pop();
							
							name = m_abc->get_multiname(index);
							obj = obj_val.to_object();
							
							if (obj)
							{
								as_value result;
								obj->get_member(name, &result);
								stack.push(result);
							}
							else
							{
								stack.push(as_value());  // undefined
							}
							
							IF_VERBOSE_ACTION(log_msg("EX: getproperty\t %s.%s = %s (rtqname)\n", obj_val.to_xstring(), name.c_str(), stack.top(0).to_xstring()));
						}
						break;
						
						default:
							// Standard case: name from pool
							{
								name = m_abc->get_multiname(index);
								obj = stack.top(0).to_object();

								if (obj)
								{
									if (obj->get_member(name, &stack.top(0)) == false)
									{
										fprintf(stderr, "[GPF_MISS] '%s' obj=%p meth=%s\n",
											name.c_str(), obj, cur_meth);
										fflush(stderr);
										stack.top(0).set_undefined();
									}
								}
								else
								{
									stack.top(0).set_undefined();
								}
							
							if (1)
							{
								static char s_gseen[512][96];
								static int s_ngseen = 0;
								int hit = -1;
								for (int k = 0; k < s_ngseen; k++)
								{
									if (strcmp(s_gseen[k], name) == 0) { hit = k; break; }
								}
								if (hit < 0 && s_ngseen < 512)
								{
									strncpy(s_gseen[s_ngseen], name, 95);
									s_gseen[s_ngseen][95] = 0;
									s_ngseen++;
									fprintf(stderr, "[GETPROP] '%s' obj=%p val=%s meth=%s\n",
										name.c_str(), obj, stack.top(0).to_xstring(), cur_meth);
									fflush(stderr);
								}
							}

							IF_VERBOSE_ACTION(log_msg("EX: getproperty\t %s, value=%s\n", name.c_str(), stack.top(0).to_xstring()));
						}
						break;
					}

					// Track which object a method reference was read from so
					// event listeners run with the correct `this`.
					if (obj != NULL && stack.size() > 0 && stack.top(0).is_function())
					{
						avm2_note_method_owner(obj, stack.top(0));
					}

					break;
				}

				case 0x68:	// initproperty, Initialize a property.
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					const char* name = m_abc->get_multiname(index);

					as_value& val = stack.top(0);
					as_object* obj = stack.top(1).to_object();
					if (obj)
					{
						{
							static char s_isseen[512][96];
							static int s_nisseen = 0;
							int hit = -1;
							for (int k = 0; k < s_nisseen; k++)
							{
							if (strcmp(s_isseen[k], name) == 0) { hit = k; break; }
						}
						if (hit < 0 && s_nisseen < 512)
						{
							strncpy(s_isseen[s_nisseen], name, 95);
							s_isseen[s_nisseen][95] = 0;
							s_nisseen++;
							fprintf(stderr, "[INITPROP] '%s' = %s obj=%p\n",
								name, val.to_xstring(), obj);
								fflush(stderr);
							}
						}
						obj->set_member(name, val);
					}

					IF_VERBOSE_ACTION(log_msg("EX: initproperty\t 0x%p.%s=%s\n", obj, name, val.to_xstring()));

					stack.drop(2);
					break;
				}

				case 0x73: //convert_i
				{
					stack.top(0).set_int( stack.top(0).to_int() );
					IF_VERBOSE_ACTION(log_msg("EX: convert_i : %i \n", stack.top(0).to_int())); 
				} break;

				case 0x80: // coerce
			{
				int index;
				ip += read_vu30( index, &m_code[ip]);
				// For now, just leave the value as-is (no-op coercion)
				// A full implementation would check the multiname type and convert
				IF_VERBOSE_ACTION(log_msg("EX: coerce %d\n", index));
			} break;

				case 0x85: // coerce_s
				{
					stack.top(0).set_string( stack.top(0).to_string() );
					IF_VERBOSE_ACTION(log_msg("EX: coerce_s : %s\n", stack.top(0).to_string())); 
				} break;

				case 0x96: // not
				{
					stack.top(0).set_bool( !stack.top(0).to_bool() );

					IF_VERBOSE_ACTION(log_msg("EX: not\n"));

				} break;

				case 0xA0:	// Add two values
				{
					if (stack.top(0).is_string() || stack.top(1).is_string())
					{
						tu_string str = stack.top(1).to_string();
						str += stack.top(0).to_string();
						stack.top(1).set_tu_string(str);
					}
					else
					{
						stack.top(1) += stack.top(0).to_number();
					}
					stack.drop(1);

					break;
				}

				case 0xA2: // multiply
				{
					stack.top(1) = stack.top(1).to_number() * stack.top(0).to_number();
					stack.drop(1);

					IF_VERBOSE_ACTION(log_msg("EX: multiply\n"));
					
					break;
				}

				case 0xAB: // equals
				{
					bool result = as_value::abstract_equality_comparison( stack.top(1), stack.top(0) );

					IF_VERBOSE_ACTION(log_msg("EX: equals %s & %s : %s\n", stack.top(0).to_xstring(), stack.top(1).to_xstring(), result? "true":"false") );

					stack.drop(1);
					stack.top(0).set_bool( result );
				} break;

				case 0xAD: //lessthan
				{
					as_value result = as_value::abstract_relational_comparison( stack.top(1), stack.top(0) );

					IF_VERBOSE_ACTION(log_msg("EX: lessthan %s & %s : %s\n", stack.top(1).to_xstring(), stack.top(0).to_xstring(), result.to_string() ) );

					stack.drop(1);
					stack.top(0) = result;
				} break;

				case 0xC2:  // inclocal_i
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);

					as_value & reg = lregister[ index ];
					reg.set_int( reg.to_int() + 1 );

					IF_VERBOSE_ACTION(log_msg("EX: inclocal_i %i\n", index ) );
					
				}break;

				case 0xD0:	// getlocal_0
				case 0xD1:	// getlocal_1
				case 0xD2:	// getlocal_2
				case 0xD3:	// getlocal_3
				{
					as_value& val = lregister[opcode & 0x03];
					stack.push(val);
					IF_VERBOSE_ACTION(log_msg("EX: getlocal_%d\t %s\n", opcode & 0x03, val.to_xstring()));
					break;
				}

				case 0xD4:	// setlocal_0
				case 0xD5:	// setlocal_1
				case 0xD6:	// setlocal_2
				case 0xD7:	// setlocal_3
				{
					lregister[opcode & 0x03] = stack.pop();

					IF_VERBOSE_ACTION(log_msg("EX: setlocal_%d\t %s\n", opcode & 0x03, lregister[opcode & 0x03].to_xstring()));
					break;
				}

				case 0x0A:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

			case 0x21:	// pushundefined (alias)
			{
				stack.push(as_value());
				break;
			}

			case 0x2E:	// pushuint
			{
				int index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				if (index >= 0 && index < m_abc->m_uinteger.size())
				{
					stack.push(as_value((double)m_abc->m_uinteger[index]));
				}
				else
				{
					stack.push(as_value(0.0));
				}
				IF_VERBOSE_ACTION(log_msg("EX: pushuint %d\n", index));
				break;
			}

			case 0x31:	// pushnamespace
			{
				int index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				// Push namespace as string for now
				if (index >= 0 && index < m_abc->m_namespace.size())
				{
					const char* ns_name = m_abc->get_string(m_abc->m_namespace[index].m_name);
					stack.push(as_value(ns_name));
				}
				else
				{
					stack.push(as_value());
				}
				IF_VERBOSE_ACTION(log_msg("EX: pushnamespace %d\n", index));
				break;
			}

			case 0x41:	// call - indirect function call
			{
				int arg_count = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						arg_count |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				
			// call opcode
			{
			}

				as_environment env(get_player());
				for (int i = 0; i < arg_count; i++)
				{
					env.push(stack.top(i));
				}
				stack.drop(arg_count);
				as_value func_val = stack.pop();
				as_value receiver = stack.pop();

				if (func_val.is_function())
				{
					as_value ret = call_method(func_val, &env, &receiver, arg_count, env.get_top_index());
					stack.push(ret);
				}
				else
				{
					stack.push(as_value());
				}
				IF_VERBOSE_ACTION(log_msg("EX: call args=%d\n", arg_count));
				break;
			}

			case 0x44:	// callstatic
			{
				int method_index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						method_index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				int arg_count = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						arg_count |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				as_environment env(get_player());
				for (int i = 0; i < arg_count; i++)
				{
					env.push(stack.top(i));
				}
				stack.drop(arg_count);
				stack.pop(); // receiver (not used for static calls)

				if (method_index >= 0 && method_index < m_abc->m_method.size())
				{
					as_value func(m_abc->m_method[method_index].get());
					as_value ret = call_method(func, &env, get_global(), arg_count, env.get_top_index());
					stack.push(ret);
				}
				else
				{
					stack.push(as_value());
				}
				IF_VERBOSE_ACTION(log_msg("EX: callstatic method=%d args=%d\n", method_index, arg_count));
				break;
			}

			case 0x4E:	// callsupervoid
			{
				int index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				int arg_count = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						arg_count |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				const char* name = m_abc->get_multiname(index);

				as_environment env(get_player());
				for (int i = 0; i < arg_count; i++)
				{
					env.push(stack.top(i));
				}
				stack.drop(arg_count);
				as_value receiver = stack.pop();

				as_object* obj = receiver.to_object();
				if (obj)
				{
					as_value super_val;
					if (obj->get_member("super", &super_val) && super_val.is_object())
					{
						as_object* super_obj = super_val.to_object();
						as_value func;
						if (super_obj->get_member(name, &func) && func.is_function())
						{
							call_method(func, &env, obj, arg_count, env.get_top_index());
						}
					}
				}
				IF_VERBOSE_ACTION(log_msg("EX: callsupervoid %s args=%d\n", name, arg_count));
				break;
			}

			case 0x95:	// typeof
			{
				as_value val = stack.pop();
				const char* type_name = "undefined";
				if (val.is_string()) type_name = "string";
				else if (val.is_number()) type_name = "number";
				else if (val.is_bool()) type_name = "boolean";
				else if (val.is_function()) type_name = "function";
				else if (val.is_object()) type_name = "object";
				else if (val.is_null()) type_name = "null";
				stack.push(as_value(type_name));
				IF_VERBOSE_ACTION(log_msg("EX: typeof %s\n", type_name));
				break;
			}

			case 0x97:	// bitnot
			{
				as_value val = stack.pop();
				stack.push(as_value((double)(~(int)val.to_number())));
				IF_VERBOSE_ACTION(log_msg("EX: bitnot\n"));
				break;
			}

				case 0x98:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

			case 0x99:	// reserved
			{
				IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
				break;
			}

				case 0x02:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x03:	// throw
				{
					as_value exc = stack.pop();
					IF_VERBOSE_ACTION(log_msg("EX: throw %s\n", exc.to_string()));
					
					// Search exception table for a matching catch handler
					bool caught = false;
					for (int i = 0; i < m_exception.size(); i++)
					{
						except_info* ei = m_exception[i].get();
						if (ip >= ei->m_from && ip < ei->m_to)
						{
							// Found a matching exception handler
							// Store exception in the catch variable
							if (ei->m_var_name >= 0 && ei->m_var_name < m_abc->m_multiname.size())
							{
								const char* var_name = m_abc->get_multiname(ei->m_var_name);
								// Set the exception variable on the scope
								if (scope.size() > 0)
								{
									scope.top(0).to_object()->set_member(var_name, exc);
								}
							}
							// Jump to catch target
							ip = ei->m_target;
							caught = true;
							break;
						}
					}
					if (!caught)
					{
						// Unhandled exception - propagate up
						result->set_undefined();
						return;
					}
					break;
				}

				case 0x04:	// getsuper
			{
				int index = 0;
				{ int _shift = 0; while (true) { Uint8 _b = m_code[ip++]; index |= (_b & 0x7F) << _shift; if ((_b & 0x80) == 0) break; _shift += 7; } }
				const char* name = m_abc->get_multiname(index);
				as_value obj_val = stack.pop();
				as_value ret;
				as_object* obj = obj_val.to_object();
				if (obj)
				{
					as_value super_val;
					if (obj->get_member("super", &super_val) && super_val.is_object())
					{
						super_val.to_object()->get_member(name, &ret);
					}
				}
				stack.push(ret);
				IF_VERBOSE_ACTION(log_msg("EX: getsuper %s\n", name));
				break;
			}

				case 0x05:	// setsuper
			{
				int index = 0;
				{ int _shift = 0; while (true) { Uint8 _b = m_code[ip++]; index |= (_b & 0x7F) << _shift; if ((_b & 0x80) == 0) break; _shift += 7; } }
				const char* name = m_abc->get_multiname(index);
				as_value value = stack.pop();
				as_value obj_val = stack.pop();
				as_object* obj = obj_val.to_object();
				if (obj)
				{
					as_value super_val;
					if (obj->get_member("super", &super_val) && super_val.is_object())
					{
						super_val.to_object()->set_member(name, value);
					}
				}
				IF_VERBOSE_ACTION(log_msg("EX: setsuper %s\n", name));
				break;
			}

				case 0x06:	// dxns
				{
					int index = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							index |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					IF_VERBOSE_ACTION(log_msg("EX: dxns %d\n", index));
					break;
				}

				case 0x07:	// dxnslate
				{
					IF_VERBOSE_ACTION(log_msg("EX: dxnslate\n"));
					stack.pop();
					break;
				}

				case 0x08:	// kill
				{
					int reg = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							reg |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					lregister[reg] = as_value();
					IF_VERBOSE_ACTION(log_msg("EX: kill %d\n", reg));
					break;
				}

				case 0x09:	// label
					break;

				case 0x0C:	// ifnlt
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					if ((!(a.to_number() < b.to_number())))
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifnlt %d\n", offset));
					break;
				}

				case 0x0D:	// ifnle
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					if ((!(a.to_number() <= b.to_number())))
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifnle %d\n", offset));
					break;
				}

				case 0x0E:	// ifngt
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					if ((!(a.to_number() > b.to_number())))
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifngt %d\n", offset));
					break;
				}

				case 0x0F:	// ifnge
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					if ((!(a.to_number() >= b.to_number())))
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifnge %d\n", offset));
					break;
				}

				case 0x10:	// jump
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: jump %d\n", offset));
					break;
				}

				case 0x13:	// ifeq
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					if (as_value::abstract_equality_comparison(a, b))
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifeq %d\n", offset));
					break;
				}

				case 0x15:	// iflt
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					if ((a.to_number() < b.to_number()))
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: iflt %d\n", offset));
					break;
				}

				case 0x16:	// ifge
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					if ((a.to_number() >= b.to_number()))
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifge %d\n", offset));
					break;
				}

				case 0x17:	// ifgt
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					if ((a.to_number() > b.to_number()))
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifgt %d\n", offset));
					break;
				}

				case 0x18:	// ifle
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					if ((a.to_number() <= b.to_number()))
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifle %d\n", offset));
					break;
				}

				case 0x19:	// ifstricteq
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					bool eq;
					if (a.is_undefined())
						eq = b.is_undefined();
					else if (b.is_undefined())
						eq = false;
					else if (a.is_null())
						eq = b.is_null();
					else if (b.is_null())
						eq = false;
					else if (a.is_string())
						eq = b.is_string() && a.to_string() == b.to_string();
					else if (a.is_bool())
						eq = b.is_bool() && a.to_bool() == b.to_bool();
					else if (a.is_object())
						eq = b.is_object() && a.to_object() == b.to_object();
					else
						eq = (a.to_number() == b.to_number());
					if (eq)
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifstricteq %d\n", offset));
					break;
				}

				case 0x1A:	// ifstrictne
				{
					int offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (offset & 0x800000) offset |= ~0xFFFFFF;
					ip += 3;
					as_value b = stack.pop();
					as_value a = stack.pop();
					bool eq;
					if (a.is_undefined())
						eq = b.is_undefined();
					else if (b.is_undefined())
						eq = false;
					else if (a.is_null())
						eq = b.is_null();
					else if (b.is_null())
						eq = false;
					else if (a.is_string())
						eq = b.is_string() && a.to_string() == b.to_string();
					else if (a.is_bool())
						eq = b.is_bool() && a.to_bool() == b.to_bool();
					else if (a.is_object())
						eq = b.is_object() && a.to_object() == b.to_object();
					else
						eq = (a.to_number() == b.to_number());
					if (!eq)
						ip += offset;
					IF_VERBOSE_ACTION(log_msg("EX: ifstrictne %d\n", offset));
					break;
				}

				case 0x1B:	// lookupswitch
				{
					// s24 default offset, u30 case_count, then (case_count+1) s24 case offsets.
					// The case/default offsets are relative to THIS instruction's opcode
					// (unlike jump/if*, which are relative to the following instruction).
					int base = ip - 1;
					int default_offset = m_code[ip] | (m_code[ip+1] << 8) | (m_code[ip+2] << 16);
					if (default_offset & 0x800000) default_offset |= ~0xFFFFFF;
					ip += 3;
					int case_count;
					ip += read_vu30(case_count, &m_code[ip]);
					int index = (int)stack.pop().to_number();
					int target = default_offset;
					if (index >= 0 && index <= case_count)
						{
							target = m_code[ip + index * 3] | (m_code[ip + index * 3 + 1] << 8) | (m_code[ip + index * 3 + 2] << 16);
							if (target & 0x800000) target |= ~0xFFFFFF;
						}
					ip += 3 * (case_count + 1);
					ip = base + target;
					IF_VERBOSE_ACTION(log_msg("EX: lookupswitch %d/%d\n", index, case_count));
					break;
				}

				case 0x1C:	// pushwith
				{
					as_value obj_val = stack.pop();
					scope.push(obj_val);
					IF_VERBOSE_ACTION(log_msg("EX: pushwith\n"));
					break;
				}

				case 0x1F:	// hasnext
			{
				// stack: obj, index -> new_index (0 if no more)
				as_value index_val = stack.pop();
				as_value obj_val = stack.pop();
				
				int index = (int)index_val.to_number();
				as_object* obj = obj_val.to_object();
				
				int next_index = 0;
				if (obj)
				{
					if (index < (int)obj->m_members.size())
					{
						next_index = index + 1;
					}
				}
				
				stack.push(as_value((double)next_index));
				IF_VERBOSE_ACTION(log_msg("EX: hasnext[%d] -> %d\n", index, next_index));
				break;
			}

				case 0x23:	// nextvalue
			{
				// stack: obj, index -> value
				as_value index_val = stack.pop();
				as_value obj_val = stack.pop();
				
				int index = (int)index_val.to_number();
				as_object* obj = obj_val.to_object();
				
				if (obj && index >= 1)
				{
					// Enumerate members and find the index-th property value
					int count = 0;
					bool found = false;
					for (stringx_hash<as_value>::const_iterator it = obj->m_members.begin(); 
						 it != obj->m_members.end(); ++it, ++count)
					{
						if (count == index - 1)  // 1-based index
						{
							stack.push(it->second);
							IF_VERBOSE_ACTION(log_msg("EX: nextvalue[%d] = %s\n", index, it->second.to_xstring()));
							found = true;
							break;
						}
					}
					if (!found)
					{
						stack.push(as_value());  // undefined
						IF_VERBOSE_ACTION(log_msg("EX: nextvalue[%d] out of range\n", index));
					}
				}
				else
				{
					stack.push(as_value());  // undefined
					IF_VERBOSE_ACTION(log_msg("EX: nextvalue invalid object or index\n"));
				}
				break;
			}

				case 0x28:	// pushnan
				{
					stack.push(as_value(NAN));
					break;
				}

				case 0x32:	// hasnext2
				{
					int obj_reg = 0;
					{ int _shift = 0; while (true) { Uint8 _b = m_code[ip++]; obj_reg |= (_b & 0x7F) << _shift; if ((_b & 0x80) == 0) break; _shift += 7; } }
					int index_reg = 0;
					{ int _shift = 0; while (true) { Uint8 _b = m_code[ip++]; index_reg |= (_b & 0x7F) << _shift; if ((_b & 0x80) == 0) break; _shift += 7; } }
					
					// Get the object and current index from registers
					as_value obj_val = lregister[obj_reg];
					int cur_index = (int)lregister[index_reg].to_number();
					
					bool has_next = false;
					as_object* obj = obj_val.to_object();
					
					if (obj)
					{
						// Check if there's a next property
						if (cur_index < (int)obj->m_members.size())
						{
							// Move to next property
							lregister[index_reg] = as_value((double)(cur_index + 1));
							has_next = true;
						}
					}
					
					stack.push(as_value(has_next));
					IF_VERBOSE_ACTION(log_msg("EX: hasnext2 obj_reg=%d index_reg=%d has_next=%d\n", obj_reg, index_reg, has_next));
					break;
				}

				case 0x33:	// pushdecimal
				{
					int unused_index;
					ip += read_vu30(unused_index, &m_code[ip]);
					stack.push(as_value(NAN));
					IF_VERBOSE_ACTION(log_msg("EX: pushdecimal (unimplemented)\n"));
					break;
				}

				case 0x34:	// pushdnan
				{
					stack.push(as_value(NAN));
					IF_VERBOSE_ACTION(log_msg("EX: pushdnan (unimplemented)\n"));
					break;
				}

				case 0x35:	// li8
				{
					as_value addr = stack.pop();
					UNUSED(addr);
					stack.push(as_value(0.0));
					IF_VERBOSE_ACTION(log_msg("EX: li8 (domain memory, unimplemented)\n"));
					break;
				}

				case 0x36:	// li16
				{
					as_value addr = stack.pop();
					UNUSED(addr);
					stack.push(as_value(0.0));
					IF_VERBOSE_ACTION(log_msg("EX: li16 (domain memory, unimplemented)\n"));
					break;
				}

				case 0x37:	// li32
				{
					as_value addr = stack.pop();
					UNUSED(addr);
					stack.push(as_value(0.0));
					IF_VERBOSE_ACTION(log_msg("EX: li32 (domain memory, unimplemented)\n"));
					break;
				}

				case 0x38:	// lf32
				{
					as_value addr = stack.pop();
					UNUSED(addr);
					stack.push(as_value(0.0));
					IF_VERBOSE_ACTION(log_msg("EX: lf32 (domain memory, unimplemented)\n"));
					break;
				}

				case 0x39:	// lf64
				{
					as_value addr = stack.pop();
					UNUSED(addr);
					stack.push(as_value(0.0));
					IF_VERBOSE_ACTION(log_msg("EX: lf64 (domain memory, unimplemented)\n"));
					break;
				}

				case 0x74:	// convert_u
				{
					as_value v = stack.pop();
					stack.push(as_value((double)(Uint32)v.to_number()));
					break;
				}

				case 0x75:	// convert_d
				{
					as_value v = stack.pop();
					stack.push(as_value(v.to_number()));
					break;
				}

				case 0x76:	// convert_b
				{
					as_value v = stack.pop();
					stack.push(as_value(v.to_bool()));
					break;
				}

				case 0x77:	// convert_o
				{
					// convert to object - pass through
					break;
				}

				case 0x82:	// coerce_a
				{
					// coerce to any - pass through
					break;
				}

				case 0x84:	// coerce_d
				{
					as_value v = stack.pop();
					stack.push(as_value(v.to_number()));
					IF_VERBOSE_ACTION(log_msg("EX: coerce_d\n"));
					break;
				}

				case 0x86:	// astype (multiname operand)
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					// value stays on the stack; no runtime type check
					IF_VERBOSE_ACTION(log_msg("EX: astype\t %s\n", m_abc->get_multiname(index)));
					break;
				}

				case 0x87:	// astypelate: stack ..., value, type -> ..., value
				{
					stack.pop();	// pop the type, keep the value
					IF_VERBOSE_ACTION(log_msg("EX: astypelate\n"));
					break;
				}

				case 0x88:	// coerce_u
				{
					as_value v = stack.pop();
					stack.push(as_value((double)(Uint32)v.to_number()));
					break;
				}

				case 0x8E:	// undefined
				{
					stack.push(as_value());
					IF_VERBOSE_ACTION(log_msg("EX: push undefined (0x8E)\n"));
					break;
				}

				case 0x8F:	// negate_p
				{
					int operand;
					ip += read_vu30(operand, &m_code[ip]);
					UNUSED(operand);
					IF_VERBOSE_ACTION(log_msg("EX: negate_p (unimplemented)\n"));
					break;
				}

				case 0x90:	// negate
				{
					as_value v = stack.pop();
					stack.push(as_value(-v.to_number()));
					break;
				}

				case 0x91:	// increment
				{
					as_value v = stack.pop();
					stack.push(as_value(v.to_number() + 1.0));
					break;
				}

				case 0x93:	// decrement
				{
					as_value v = stack.pop();
					stack.push(as_value(v.to_number() - 1.0));
					break;
				}

				case 0xA1:	// subtract
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value(a.to_number() - b.to_number()));
					break;
				}

				case 0xA3:	// divide
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					if (b.to_number() != 0)
						stack.push(as_value(a.to_number() / b.to_number()));
					else
						stack.push(as_value(NAN));
					break;
				}

				case 0xA4:	// modulo
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value(fmod(a.to_number(), b.to_number())));
					break;
				}

				case 0xA5:	// lshift
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value((double)((int)a.to_number() << ((int)b.to_number() & 31))));
					break;
				}

				case 0xA6:	// rshift
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value((double)((int)a.to_number() >> ((int)b.to_number() & 31))));
					break;
				}

				case 0xA7:	// urshift
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value((double)((Uint32)a.to_number() >> ((int)b.to_number() & 31))));
					break;
				}

				case 0xA8:	// bitand
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value((double)((int)a.to_number() & (int)b.to_number())));
					break;
				}

				case 0xA9:	// bitor
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value((double)((int)a.to_number() | (int)b.to_number())));
					break;
				}

				case 0xAA:	// bitxor
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value((double)((int)a.to_number() ^ (int)b.to_number())));
					break;
				}

				case 0xAC:	// strictequals
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value(a.to_string() == b.to_string()));
					break;
				}

				case 0xAE:	// lessequals
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value(a.to_number() <= b.to_number()));
					break;
				}

				case 0xAF:	// greaterthan
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value(a.to_number() > b.to_number()));
					break;
				}

				case 0xB0:	// greaterequals
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value(a.to_number() >= b.to_number()));
					break;
				}

				case 0xB1:	// instanceof
			{
				as_value type_val = stack.pop();
				as_value obj_val = stack.pop();
				bool result = false;
				as_object* obj = obj_val.to_object();
				as_object* type = type_val.to_object();
				if (obj && type)
				{
					// Simple check: compare class names
					as_value obj_class, type_class;
					if (obj->get_member("constructor", &obj_class) && type->get_member("constructor", &type_class))
					{
						// For now, just check if they're the same object
						result = (obj_class.to_object() == type_class.to_object());
					}
				}
				stack.push(as_value(result));
				IF_VERBOSE_ACTION(log_msg("EX: instanceof\n"));
				break;
			}

				case 0xB2:	// istype
			{
				// Stack: value -> true/false
				int index = 0;
				{ int _shift = 0; while (true) { Uint8 _b = m_code[ip++]; index |= (_b & 0x7F) << _shift; if ((_b & 0x80) == 0) break; _shift += 7; } }
				const char* type_name = m_abc->get_multiname(index);
				as_value val = stack.top(0);
				bool result = false;
				
				// Check type based on name
				if (val.is_string() && strcmp(type_name, "String") == 0) result = true;
				else if (val.is_number() && (strcmp(type_name, "Number") == 0 || strcmp(type_name, "int") == 0 || strcmp(type_name, "uint") == 0)) result = true;
				else if (val.is_bool() && strcmp(type_name, "Boolean") == 0) result = true;
				else if (val.is_object() && strcmp(type_name, "Object") == 0) result = true;
				else if (val.is_object()) result = true;  // Object matches any class type (simplified)
				
				stack.top(0) = as_value(result);
				IF_VERBOSE_ACTION(log_msg("EX: istype %s -> %d\n", type_name, result));
				break;
			}

				case 0xB3:	// istypelate
				{
					// Stack: value, type -> result
					as_value type_val = stack.pop();
					as_value val = stack.pop();
					bool result = false;
					as_object* type = type_val.to_object();
					if (type)
						{
							// Runtime class name a primitive value belongs to.
							const char* expect = NULL;
							if (val.is_string()) expect = "String";
							else if (val.is_bool()) expect = "Boolean";
							else if (val.is_number()) expect = "Number";

							// 1) real object instance: match along the prototype chain
							if (val.is_object())
							{
								for (as_object* o = val.to_object(); o != NULL; o = o->get_proto())
								{
									if (o == type) { result = true; break; }
								}
							}

							// 2) primitive (or primitive stored as an object): the value
							//    belongs to its own class and to Object.  Without this
							//    `label is int` used to succeed for string labels, which
							//    sent getFrameIndexByLabel() down the numeric path and
							//    produced a garbage frame index.
							if (!result && expect != NULL)
							{
								as_object* global = get_global();
								if (global)
								{
									const char* names[] = {
										"Object", "String", "int", "uint",
										"Number", "Boolean", "Function", "Array", NULL };

									for (int ni = 0; names[ni]; ni++)
									{
										as_value cls;
										if (global->get_member(names[ni], &cls) &&
											cls.to_object() == type)
										{
											if (strcmp(names[ni], "Object") == 0 ||
												strcmp(names[ni], expect) == 0)
											{
												result = true;
											}
											else if (val.is_number() &&
												(strcmp(names[ni], "int") == 0 ||
												 strcmp(names[ni], "uint") == 0))
											{
												// int/uint are Number-compatible
												result = true;
											}
											break;
										}
									}
								}
							}
						}
					stack.push(as_value(result));
					IF_VERBOSE_ACTION(log_msg("EX: istypelate -> %d\n", result));
					break;
				}

				case 0xC0:	// increment_i
				{
					as_value v = stack.pop();
					stack.push(as_value((double)((int)v.to_number() + 1)));
					break;
				}

				case 0xC1:	// decrement_i
				{
					as_value v = stack.pop();
					stack.push(as_value((double)((int)v.to_number() - 1)));
					break;
				}

				case 0xC3:	// declocal_i
				{
					int reg = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							reg |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					lregister[reg] = as_value((double)((int)lregister[reg].to_number() - 1));
					IF_VERBOSE_ACTION(log_msg("EX: declocal_i %d\n", reg));
					break;
				}

				case 0xC4:	// negate_i
				{
					as_value v = stack.pop();
					stack.push(as_value((double)(-(int)v.to_number())));
					break;
				}

				case 0xC5:	// add_i
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value((double)((int)a.to_number() + (int)b.to_number())));
					break;
				}

				case 0xC6:	// subtract_i
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value((double)((int)a.to_number() - (int)b.to_number())));
					break;
				}

				case 0xC7:	// multiply_i
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value((double)((int)a.to_number() * (int)b.to_number())));
					break;
				}

				// === Object operations ===
				case 0x40:	// newfunction - create closure
			{
				int index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				if (index >= 0 && index < m_abc->m_method.size())
				{
					stack.push(as_value(m_abc->m_method[index].get()));
				}
				else
				{
					stack.push(as_value());
				}
				IF_VERBOSE_ACTION(log_msg("EX: newfunction %d\n", index));
				break;
			}

				case 0x42:	// construct - dynamic construction
			{
				int arg_count = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						arg_count |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				as_environment env(get_player());
				for (int i = 0; i < arg_count; i++)
				{
					env.push(stack.top(i));
				}
				stack.drop(arg_count);
				as_value ctor = stack.pop();

			if (ctor.is_function())
				{
					as_value ret = call_method(ctor, &env, as_value(), arg_count, env.get_top_index());
					stack.push(ret);
				}
				else
				{
					as_object* cls_obj = ctor.to_object();
					as_class* cls = cls_obj ? cast_to<as_class>(cls_obj) : NULL;
					if (cls != NULL && cls->get_class_index() >= 0)
					{
						// An ABC class object: build an instance and run its
						// instance initializer (iinit), like constructprop does.
						instance_info* ii = m_abc->get_instance_info_by_index(cls->get_class_index());
						const char* class_name = ii ? m_abc->get_multiname(ii->m_name) : NULL;

						gc_ptr<as_object> new_object;
						if (class_name && avm2_class_extends_display(m_abc.get_ptr(), ii))
						{
							sprite_definition* empty_def = new sprite_definition(get_player(), NULL);
							sprite_instance* sprite = new sprite_instance(
								get_player(), empty_def,
								lregister[0].to_object() ? lregister[0].to_object()->get_root() : NULL,
								cast_to<sprite_instance>(lregister[0].to_object()),
								0);
							sprite->set_name(class_name);
							new_object = sprite;

							as_graphics* gfx = new as_graphics(get_player(), sprite);
							sprite->set_member("graphics", as_value(gfx));
						}
						else
						{
							new_object = new as_object(get_player());
							fprintf(stderr, "[BIRTH-C] obj=%p name='%s' super_idx=%d\n",
								(void*) new_object.get_ptr(), class_name ? class_name : "?",
								ii ? (int) ii->m_super_name : -1);
							fflush(stderr);
						}

						new_object->set_instance(ii);
						new_object->m_proto = cls_obj;

						as_function* c = class_name ? m_abc->get_class_constructor(tu_string(class_name)) : NULL;
						if (c)
						{
							call_method(c, &env, new_object.get_ptr(), arg_count, env.get_top_index());
						}
						stack.push(new_object.get_ptr());
					}
					else if (cls_obj != NULL)
					{
						// Non-function objects (e.g. result of applytype like Vector.<Class>)
						// Just re-push the object - it was already created (e.g. by applytype)
						stack.push(ctor);
					}
					else
					{
						stack.push(as_value());
					}
				}
				IF_VERBOSE_ACTION(log_msg("EX: construct args=%d\n", arg_count));
				break;
			}

				case 0x43:	// callmethod
			{
				int disp_id = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						disp_id |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				int arg_count = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						arg_count |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				as_environment env(get_player());
				for (int i = 0; i < arg_count; i++)
				{
					env.push(stack.top(i));
				}
				stack.drop(arg_count);
				as_value receiver = stack.pop();

				// Try to find method by dispatch id on the receiver object
				as_value func;
				as_object* obj = receiver.to_object();
				if (obj)
				{
					// Try to get method by name from the multiname table
					if (disp_id >= 0 && disp_id < m_abc->m_multiname.size())
					{
						const char* method_name = m_abc->get_multiname(disp_id);
						if (obj->get_member(method_name, &func) && func.is_function())
						{
							as_value ret = call_method(func, &env, obj, arg_count, env.get_top_index());
							stack.push(ret);
						}
						else
						{
							stack.push(as_value());
						}
					}
					else
					{
						stack.push(as_value());
					}
				}
				else
				{
					stack.push(as_value());
				}
				IF_VERBOSE_ACTION(log_msg("EX: callmethod disp_id=%d args=%d\n", disp_id, arg_count));
				break;
			}

				case 0x45:	// callsuper
			{
				int index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				int arg_count = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						arg_count |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				const char* name = m_abc->get_multiname(index);

				as_environment env(get_player());
				for (int i = 0; i < arg_count; i++)
				{
					env.push(stack.top(i));
				}
				stack.drop(arg_count);
				as_value receiver = stack.pop();

				// Find super class and call method on it
				as_object* obj = receiver.to_object();
				as_value ret;
				if (obj)
				{
					// Walk up the prototype chain to find the method in super class
					as_value super_val;
					if (obj->get_member("super", &super_val) && super_val.is_object())
					{
						as_object* super_obj = super_val.to_object();
						as_value func;
						if (super_obj->get_member(name, &func) && func.is_function())
						{
							ret = call_method(func, &env, obj, arg_count, env.get_top_index());
						}
					}
				}
				stack.push(ret);
				IF_VERBOSE_ACTION(log_msg("EX: callsuper %s args=%d\n", name, arg_count));
				break;
			}

				case 0x4C:	// callproplex - call property, push the result
				{
					int index;
					ip += read_vu30(index, &m_code[ip]);
					const char* name = m_abc->get_multiname(index);
					int arg_count;
					ip += read_vu30(arg_count, &m_code[ip]);
					as_environment env(get_player());
					for (int i = 0; i < arg_count; i++)
						env.push(stack.top(i));
					stack.drop(arg_count);
					as_value receiver = stack.top(0);
					as_value result;
					result.set_undefined();
					as_value func;
					if (receiver.find_property(name, &func))
						result = call_method(func, &env, receiver, arg_count, env.get_top_index());
					IF_VERBOSE_ACTION(log_msg("EX: callproplex\t %s(args:%d)\n", name, arg_count));
					stack.drop(1);
					stack.push(result);
					break;
				}

			case 0x53:	// applytype - create parameterized type (e.g. Vector.<Class>)
			{
				int arg_count = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						arg_count |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				// Pop type arguments from stack (e.g. Class for Vector.<Class>)
				for (int i = 0; i < arg_count; i++)
				{
					stack.pop();
				}
				// The base type (e.g. Vector) is now on top of stack
				// For Vector types, create an empty array as the result
				// This is a simplification - in a full implementation,
				// we'd create a proper parameterized type
				as_value base_type = stack.pop();
				if (arg_count > 0)
				{
					// Create an empty array for Vector.<T> types
					as_array* arr = new as_array(get_player());
					stack.push(as_value(arr));
				}
				else
				{
					// No type args, just push base type back
					stack.push(base_type);
				}
				IF_VERBOSE_ACTION(log_msg("EX: applytype %d\n", arg_count));
				break;
			}

			case 0x55:	// newobject (standard AVM2 opcode for 0x55)
			{
				int arg_count = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						arg_count |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				as_object* obj = new as_object(get_player());
				// Stack: ..., key1, value1, key2, value2, ... (in reverse order from top)
				for (int i = 0; i < arg_count; i++)
				{
					as_value val = stack.pop();
					as_value key = stack.pop();
					if (key.is_string())
					{
						obj->set_member(key.to_string(), val);
					}
				}
				stack.push(as_value(obj));
				IF_VERBOSE_ACTION(log_msg("EX: newobject %d\n", arg_count));
				break;
			}

				case 0x57:	// newactivation
				{
					IF_VERBOSE_ACTION(log_msg("EX: newactivation\n"));
					as_object* obj = new as_object(get_player());
					stack.push(as_value(obj));
					break;
				}

				case 0x59:	// getdescendants
			{
				int index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				stack.pop(); // pop obj
				stack.push(as_value()); // return undefined
				IF_VERBOSE_ACTION(log_msg("EX: getdescendants %d\n", index));
				break;
			}

				case 0x5A:	// newcatch
			{
				int index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				// Create a catch scope object
				as_object* catch_obj = new as_object(get_player());
				stack.push(as_value(catch_obj));
				IF_VERBOSE_ACTION(log_msg("EX: newcatch %d\n", index));
				break;
			}

				case 0x5C:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x64:	// getglobalscope
				{
					IF_VERBOSE_ACTION(log_msg("EX: getglobalscope\n"));
					stack.push(lregister[0]); // this
					break;
				}

				case 0x67:	// getouterscope
				{
					int unused_index;
					ip += read_vu30(unused_index, &m_code[ip]);
					stack.push(scope.size() ? scope.top(0) : as_value());
					IF_VERBOSE_ACTION(log_msg("EX: getouterscope (unimplemented)\n"));
					break;
				}

				case 0x69:	// setpropertylate
				{
					int unused_index;
					ip += read_vu30(unused_index, &m_code[ip]);
					as_value obj_val = stack.pop();
					UNUSED(obj_val);
					as_value value = stack.pop();
					UNUSED(value);
					IF_VERBOSE_ACTION(log_msg("EX: setpropertylate (unimplemented)\n"));
					break;
				}

				case 0x6A:	// deleteproperty
			{
				int index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				const char* name = m_abc->get_multiname(index);
				as_value obj_val = stack.pop();
				bool deleted = false;
				as_object* obj = obj_val.to_object();
				if (obj)
				{
					// Actually remove the member from the object's m_members hash
					// instead of just setting it to undefined
					as_value dummy;
					if (obj->m_members.get(name, &dummy))
					{
						obj->m_members.erase(name);
						deleted = true;
					}
				}
				stack.push(as_value(deleted));
				IF_VERBOSE_ACTION(log_msg("EX: deleteproperty %s\n", name));
				break;
			}

				case 0x6B:	// deletepropertylate
				{
					int unused_index;
					ip += read_vu30(unused_index, &m_code[ip]);
					as_value obj_val = stack.pop();
					UNUSED(obj_val);
					stack.push(as_value(false));
					IF_VERBOSE_ACTION(log_msg("EX: deletepropertylate (unimplemented)\n"));
					break;
				}

				case 0x6C:	// getslot
			{
				int slot_index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						slot_index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				as_value obj_val = stack.pop();
				as_value ret;
				as_object* obj = obj_val.to_object();
				if (obj)
				{
					char slot_name[32];
					snprintf(slot_name, sizeof(slot_name), "__slot_%d__", slot_index);
					// Slot access should NOT walk the prototype chain.
					// Slots are instance-local storage defined by instance traits.
					obj->m_members.get(slot_name, &ret);
				}
				stack.push(ret);
				IF_VERBOSE_ACTION(log_msg("EX: getslot %d\n", slot_index));
				break;
			}

				case 0x6D:	// setslot
			{
				int slot_index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						slot_index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				as_value value = stack.pop();
				as_value obj_val = stack.pop();
				as_object* obj = obj_val.to_object();
				if (obj)
				{
					char slot_name[32];
					snprintf(slot_name, sizeof(slot_name), "__slot_%d__", slot_index);
					obj->set_member(slot_name, value);
				}
				IF_VERBOSE_ACTION(log_msg("EX: setslot %d\n", slot_index));
				break;
			}

				case 0x94:	// declocal
				{
					int reg = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							reg |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					lregister[reg] = as_value((int)lregister[reg].to_number() - 1);
					IF_VERBOSE_ACTION(log_msg("EX: declocal %d\n", reg));
					break;
				}

				case 0xD8:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xD9:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xDA:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xDB:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xDC:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xDD:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xDE:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xDF:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xE0:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xE1:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xE2:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xE3:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xE4:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xE5:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xE6:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xE7:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xEF:	// debug
				{
					int debug_type = m_code[ip++];
					int index = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							index |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					Uint8 reg = m_code[ip++];
					int extra = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							extra |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					break;
				}

				case 0xF0:	// debugline
				{
					int line = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							line |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					break;
				}

				case 0xF1:	// debugfile
				{
					int index = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							index |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					break;
				}

				case 0xF2:	// bkptline
				{
					int line = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							line |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					break;
				}

				// Missing opcodes that may be encountered
				case 0x00:	// nop
				{
					// No-op for now
					break;
				}

				case 0x01:	// bkpt
				{
					// No operation
					break;
				}

				case 0x1E:	// nextname
			{
				// stack: obj, index -> name
				as_value index_val = stack.pop();
				as_value obj_val = stack.pop();
				
				int index = (int)index_val.to_number();
				as_object* obj = obj_val.to_object();
				
				if (obj && index >= 1)
				{
					// Enumerate members and find the index-th property
					int count = 0;
					bool found = false;
					for (stringx_hash<as_value>::const_iterator it = obj->m_members.begin(); 
						 it != obj->m_members.end(); ++it, ++count)
					{
						if (count == index - 1)  // 1-based index
						{
							stack.push(as_value(it->first.c_str()));
							IF_VERBOSE_ACTION(log_msg("EX: nextname[%d] = '%s'\n", index, it->first.c_str()));
							found = true;
							break;
						}
					}
					if (!found)
					{
						stack.push(as_value());  // undefined
						IF_VERBOSE_ACTION(log_msg("EX: nextname[%d] out of range\n", index));
					}
				}
				else
				{
					stack.push(as_value());  // undefined
					IF_VERBOSE_ACTION(log_msg("EX: nextname invalid object or index\n"));
				}
				break;
			}

				case 0x22:	// pushfloat
				{
					int index = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							index |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					// Push undefined for unknown constant
					stack.push(as_value());
					IF_VERBOSE_ACTION(log_msg("EX: pushconstant %d\n", index));
					break;
				}

				case 0x2B:	// swap
				{
					as_value a = stack.pop();
					as_value b = stack.pop();
					stack.push(a);
					stack.push(b);
					break;
				}

				case 0x6E:	// getglobalslot
			{
				int slot_index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						slot_index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				as_value ret;
				as_object* global = get_global();
				if (global)
				{
					char slot_name[32];
					snprintf(slot_name, sizeof(slot_name), "__slot_%d__", slot_index);
					global->get_member(slot_name, &ret);
				}
				stack.push(ret);
				IF_VERBOSE_ACTION(log_msg("EX: getglobalslot %d\n", slot_index));
				break;
			}

				case 0x6F:	// setglobalslot
			{
				int slot_index = 0;
				{
					int _shift = 0;
					while (true) {
						Uint8 _b = m_code[ip++];
						slot_index |= (_b & 0x7F) << _shift;
						if ((_b & 0x80) == 0) break;
						_shift += 7;
					}
				}
				as_value value = stack.pop();
				as_object* global = get_global();
				if (global)
				{
					char slot_name[32];
					snprintf(slot_name, sizeof(slot_name), "__slot_%d__", slot_index);
					global->set_member(slot_name, value);
				}
				IF_VERBOSE_ACTION(log_msg("EX: setglobalslot %d\n", slot_index));
				break;
			}

				case 0x78:	// checkfilter
				{
					IF_VERBOSE_ACTION(log_msg("EX: checkfilter (no-op)\n"));
					break;
				}

				case 0x79:	// convert_f
				{
					as_value v = stack.pop();
					stack.push(as_value((double)(float)v.to_number()));
					IF_VERBOSE_ACTION(log_msg("EX: convert_f\n"));
					break;
				}

				case 0x7A:	// unplus
				{
					as_value v = stack.pop();
					stack.push(as_value(v.to_number()));
					IF_VERBOSE_ACTION(log_msg("EX: unplus\n"));
					break;
				}

				case 0x7B:	// convert_f
				{
					// Same as convert_d for our purposes
					as_value v = stack.pop();
					stack.push(as_value(v.to_number()));
					IF_VERBOSE_ACTION(log_msg("EX: convert_f\n"));
					break;
				}

				case 0x7C:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x7D:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x83:	// coerce_i
				{
					as_value v = stack.pop();
					stack.push(as_value((double)v.to_int()));
					IF_VERBOSE_ACTION(log_msg("EX: coerce_i\n"));
					break;
				}

				case 0x89:	// coerce_o
				{
					as_value v = stack.pop();
					if (v.is_undefined())
					{
						stack.push(as_value());	// undefined -> null
					}
					else
					{
						stack.push(v);
					}
					IF_VERBOSE_ACTION(log_msg("EX: coerce_o\n"));
					break;
				}

				case 0x8A:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x8B:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x8C:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x8D:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xB4:	// in
			{
				as_value obj_val = stack.pop();
				as_value name_val = stack.pop();
				bool result = false;
				as_object* obj = obj_val.to_object();
				if (obj && name_val.is_string())
				{
					as_value val;
					result = obj->get_member(name_val.to_string(), &val);
				}
				stack.push(as_value(result));
				IF_VERBOSE_ACTION(log_msg("EX: in\n"));
				break;
			}

				case 0xB5:	// reserved
				{
					int operand;
					ip += read_vu30(operand, &m_code[ip]);
					UNUSED(operand);
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xB6:	// reserved
				{
					int index = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							index |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					const char* name = m_abc->get_multiname(index);
					as_value val;
					as_object* global = get_global();
					if (global) global->get_member(name, &val);
					stack.push(val);
					IF_VERBOSE_ACTION(log_msg("EX: finddef %s\n", name));
					break;
				}

				case 0xB7:	// reserved
				{
					int index = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							index |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					const char* name = m_abc->get_multiname(index);
					as_value val;
					as_object* global = get_global();
					if (global) global->get_member(name, &val);
					stack.push(val);
					IF_VERBOSE_ACTION(log_msg("EX: getlex %s\n", name));
					break;
				}

				case 0xB8:	// reserved
				{
					// Coerce top of stack to the given type
					int index = 0;
					{ int _shift = 0; while (true) { Uint8 _b = m_code[ip++]; index |= (_b & 0x7F) << _shift; if ((_b & 0x80) == 0) break; _shift += 7; } }
					const char* type_name = m_abc->get_multiname(index);
					// For now, just leave the value on stack (type coercion is simplified)
					IF_VERBOSE_ACTION(log_msg("EX: coerce %s\n", type_name));
					break;
				}

				case 0xB9:	// reserved
				{
					int operand;
					ip += read_vu30(operand, &m_code[ip]);
					UNUSED(operand);
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xBA:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xC8:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xC9:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xCA:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xCB:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xCC:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xCD:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xCE:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xCF:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x0B:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x3A:	// si8
				{
					as_value val = stack.pop();
					UNUSED(val);
					as_value addr = stack.pop();
					UNUSED(addr);
					IF_VERBOSE_ACTION(log_msg("EX: si8 (domain memory, unimplemented)\n"));
					break;
				}

				case 0x3B:	// si16
				{
					as_value val = stack.pop();
					UNUSED(val);
					as_value addr = stack.pop();
					UNUSED(addr);
					IF_VERBOSE_ACTION(log_msg("EX: si16 (domain memory, unimplemented)\n"));
					break;
				}

				case 0x3C:	// si32
				{
					as_value val = stack.pop();
					UNUSED(val);
					as_value addr = stack.pop();
					UNUSED(addr);
					IF_VERBOSE_ACTION(log_msg("EX: si32 (domain memory, unimplemented)\n"));
					break;
				}

				case 0x3D:	// sf32
				{
					as_value val = stack.pop();
					UNUSED(val);
					as_value addr = stack.pop();
					UNUSED(addr);
					IF_VERBOSE_ACTION(log_msg("EX: sf32 (domain memory, unimplemented)\n"));
					break;
				}

				case 0x3E:	// sf64
				{
					as_value val = stack.pop();
					UNUSED(val);
					as_value addr = stack.pop();
					UNUSED(addr);
					IF_VERBOSE_ACTION(log_msg("EX: sf64 (domain memory, unimplemented)\n"));
					break;
				}

				case 0x3F:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x4B:	// callsuperid
				{
					int operand;
					ip += read_vu30(operand, &m_code[ip]);
					ip += read_vu30(operand, &m_code[ip]);
					UNUSED(operand);
					IF_VERBOSE_ACTION(log_msg("EX: callsuperid (unimplemented)\n"));
					break;
				}

				case 0x4D:	// callinterface
				{
					int operand;
					ip += read_vu30(operand, &m_code[ip]);
					ip += read_vu30(operand, &m_code[ip]);
					UNUSED(operand);
					IF_VERBOSE_ACTION(log_msg("EX: callinterface (unimplemented)\n"));
					break;
				}

			case 0x50:	// sxi1 - sign extend 1-bit value
			{
				as_value v = stack.pop();
				int x = ((int)v.to_number()) & 1;
				stack.push(as_value(x ? -1.0 : 0.0));
				IF_VERBOSE_ACTION(log_msg("EX: sxi1\n"));
				break;
			}

			case 0x51:	// sxi8 - sign extend 8-bit value
			{
				as_value v = stack.pop();
				stack.push(as_value((double)(int8_t)(int)v.to_number()));
				IF_VERBOSE_ACTION(log_msg("EX: sxi8\n"));
				break;
			}

			case 0x52:	// sxi16 - sign extend 16-bit value
			{
				as_value v = stack.pop();
				stack.push(as_value((double)(int16_t)(int)v.to_number()));
				IF_VERBOSE_ACTION(log_msg("EX: sxi16\n"));
				break;
			}

			case 0x54:	// pushfloat4
			{
				int operand;
				ip += read_vu30(operand, &m_code[ip]);
				ip += read_vu30(operand, &m_code[ip]);
				UNUSED(operand);
				stack.push(as_value());
				IF_VERBOSE_ACTION(log_msg("EX: pushfloat4 (unimplemented)\n"));
				break;
			}

				case 0x5B:	// deldescendants
				{
					int operand;
					ip += read_vu30(operand, &m_code[ip]);
					UNUSED(operand);
					IF_VERBOSE_ACTION(log_msg("EX: deldescendants (unimplemented)\n"));
					break;
				}

				case 0x70:	// convert_s
				{
					as_value v = stack.pop();
					stack.push(as_value(v.to_string()));
					IF_VERBOSE_ACTION(log_msg("EX: convert_s\n"));
					break;
				}

				case 0x71:	// esc_xelem
				{
					as_value v = stack.pop();
					stack.push(as_value(v.to_string()));
					IF_VERBOSE_ACTION(log_msg("EX: esc_xelem\n"));
					break;
				}

				case 0x72:	// esc_xattr
				{
					as_value v = stack.pop();
					stack.push(as_value(v.to_string()));
					IF_VERBOSE_ACTION(log_msg("EX: esc_xattr\n"));
					break;
				}

				case 0x7E:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x7F:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0x81:	// coerce_b
				{
					as_value v = stack.pop();
					stack.push(as_value(v.to_bool()));
					IF_VERBOSE_ACTION(log_msg("EX: coerce_b\n"));
					break;
				}

				case 0x92:	// inclocal
				{
					int reg = 0;
					{
						int _shift = 0;
						while (true) {
							Uint8 _b = m_code[ip++];
							reg |= (_b & 0x7F) << _shift;
							if ((_b & 0x80) == 0) break;
							_shift += 7;
						}
					}
					lregister[reg] = as_value(lregister[reg].to_number() + 1.0);
					IF_VERBOSE_ACTION(log_msg("EX: inclocal %d\n", reg));
					break;
				}

				case 0x9A:	// concat
				{
					IF_VERBOSE_ACTION(log_msg("EX: concat (unimplemented)\n"));
					break;
				}

				case 0x9B:	// add_d
				{
					as_value b = stack.pop();
					as_value a = stack.pop();
					stack.push(as_value(a.to_number() + b.to_number()));
					IF_VERBOSE_ACTION(log_msg("EX: add_d\n"));
					break;
				}

				case 0x9C:	// increment_p
				{
					int operand;
					ip += read_vu30(operand, &m_code[ip]);
					UNUSED(operand);
					IF_VERBOSE_ACTION(log_msg("EX: increment_p (unimplemented)\n"));
					break;
				}

				case 0x9D:	// inclocal_p
				{
					int operand;
					ip += read_vu30(operand, &m_code[ip]);
					ip += read_vu30(operand, &m_code[ip]);
					UNUSED(operand);
					IF_VERBOSE_ACTION(log_msg("EX: inclocal_p (unimplemented)\n"));
					break;
				}

				case 0x9E:	// decrement_p
				{
					int operand;
					ip += read_vu30(operand, &m_code[ip]);
					UNUSED(operand);
					IF_VERBOSE_ACTION(log_msg("EX: decrement_p (unimplemented)\n"));
					break;
				}

				case 0x9F:	// declocal_p
				{
					int operand;
					ip += read_vu30(operand, &m_code[ip]);
					ip += read_vu30(operand, &m_code[ip]);
					UNUSED(operand);
					IF_VERBOSE_ACTION(log_msg("EX: declocal_p (unimplemented)\n"));
					break;
				}

				case 0xBB:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xBC:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xBD:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xBE:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xBF:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xE8:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xE9:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xEA:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xEB:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xEC:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xED:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xEE:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xF3:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xF4:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xF5:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xF6:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xF7:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xF8:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xF9:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xFA:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xFB:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xFC:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xFD:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xFE:	// reserved
				{
					IF_VERBOSE_ACTION(log_msg("EX: reserved opcode 0x%02X\n", opcode));
					break;
				}

				case 0xFF:
				{
					IF_VERBOSE_ACTION(log_msg("EX: unknown/extended opcode 0x%02X\n", opcode));
					break;
				}

				default:
				{
					log_error("Unknown AVM2 opcode: 0x%02X at ip=%d\n", opcode, ip - 1);
					break;
				}
			}

		}
		while (ip < m_code.size());
	}

	void as_3_function::read(stream* in)
	// read method_info
	{
		int param_count = in->read_vu30();

		// The return_type field is an index into the multiname
		m_return_type = in->read_vu30();

		m_param_type.resize(param_count);
		for (int i = 0; i < param_count; i++)
		{
			m_param_type[i] = in->read_vu30();
		}

		m_name = in->read_vu30();
		m_flags = in->read_u8();

		if (m_flags & HAS_OPTIONAL)
		{
			int option_count = in->read_vu30();
			m_options.resize(option_count);

			for (int o = 0; o < option_count; ++o)
			{
				m_options[o].m_value = in->read_vu30();
				m_options[o].m_kind = in->read_u8();
			}
		}

		if (m_flags & HAS_PARAM_NAMES)
		{
			// param_info: u30 param_count, u30 param_name[param_count]
			int param_name_count = in->read_vu30();
			for (int p = 0; p < param_name_count; p++)
			{
				in->read_vu30(); // skip param name index
			}
		}

		IF_VERBOSE_PARSE(log_msg("method_info: name='%s', type='%s', params=%d\n",
			m_abc->get_string(m_name), m_abc->get_multiname(m_return_type), m_param_type.size()));
	}

	void as_3_function::read_body(stream* in)
	// read body_info
	{
		IF_VERBOSE_PARSE(log_msg("body_info[%d]\n", m_method));

		m_max_stack = in->read_vu30();
		m_local_count = in->read_vu30();
		m_init_scope_depth = in->read_vu30();
		m_max_scope_depth = in->read_vu30();

		int i, n;
		n = in->read_vu30();	// code_length
		m_code.resize(n);
		for (i = 0; i < n; i++)
		{
			m_code[i] = in->read_u8();
		}

		n = in->read_vu30();	// exception_count
		m_exception.resize(n);
		for (i = 0; i < n; i++)
		{
			except_info* e = new except_info();
			e->read(in, m_abc.get_ptr());
			m_exception[i] = e;
		}

		n = in->read_vu30();	// trait_count
		m_trait.resize(n);
		for (int i = 0; i < n; i++)
		{
			traits_info* trait = new traits_info();
			trait->read(in, m_abc.get_ptr());
			m_trait[i] = trait;
		}

		IF_VERBOSE_PARSE(log_msg("method	%i\n", m_method));
		IF_VERBOSE_PARSE(log_disasm_avm2(m_code, m_abc.get_ptr()));

	}

	tu_string as_3_function::get_multiname(int index, vm_stack & stack) const
	{
		multiname::kind kind = (multiname::kind)m_abc->get_multiname_type( index );
		switch( kind )
		{
		case multiname::CONSTANT_MultinameL:
		case multiname::CONSTANT_MultinameLA:
			// Late-bound name from stack
			if (stack.top(0).is_string() || stack.top(0).is_number())
			{
				return stack.pop().to_string();
			}
			else
			{
				stack.pop();
				return "";
			}
			break;
			
		case multiname::CONSTANT_RTQNameL:
		case multiname::CONSTANT_RTQNameLA:
			// Runtime qualified name late - name from stack
			if (stack.top(0).is_string() || stack.top(0).is_number())
			{
				return stack.pop().to_string();
			}
			else
			{
				stack.pop();
				return "";
			}
			break;
			
		case multiname::CONSTANT_RTQName:
		case multiname::CONSTANT_RTQNameA:
			// Runtime qualified name - ns from stack, name from pool
			{
				stack.pop();  // pop namespace
				return m_abc->get_multiname(index);
			}
			break;
			
		case multiname::CONSTANT_TypeName:
			// Type parameterized name - get base name
			return m_abc->get_multiname(index);
			
		case multiname::CONSTANT_Multiname:
		case multiname::CONSTANT_MultinameA:
		case multiname::CONSTANT_QName:
		case multiname::CONSTANT_QNameA:
			return m_abc->get_multiname(index);
			
		default:
			IF_VERBOSE_ACTION(log_msg("get_multiname: unhandled kind 0x%02X, using pool name\n", kind));
			return m_abc->get_multiname(index);
		}
	}
}

