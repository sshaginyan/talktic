#include "jsint.h"
#include "pwm.h"
#include "device_ad.h"
#include "device_port.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#define delay_us _delay_us
#define delay_ms _delay_ms

#include <util/delay.h>

static void
analogRead_global_method(JSVirtualMachine * vm, JSBuiltinInfo * builtin_info,
				  void *instance_context, JSNode * result_return, JSNode * args)
{
	unsigned int val;
	JSInt32 pin;

	result_return->type = JS_FLOAT;
	result_return->u.vfloat = 0;

	if (args->u.vinteger == 1) {
		pin = js_vm_to_int32(vm, &args[1]);
		if(pin < 0 || pin > 7) {
			DEVICE_ADC_UNINIT();
			return;
		}

		if(! DEVICE_ADC_CHECK()) {
			DEVICE_ADC_INIT();
			DEVICE_ADC_ENABLE();
		}

    	DEVICE_ADC_SET_CHANNEL(pin);
		delay_us(7);

		DEVICE_ADC_SAMPLE_SINGLE();
		DEVICE_ADC_GET_SAMPLE_10BIT(val);

		result_return->u.vfloat = (JSFloat)val / 0x3ff;
	}
}


static unsigned char s_analog_pwm = 0;
static unsigned char s_analog_value[8];

extern unsigned char device_port_out(unsigned char pin, unsigned char mode);

#define TCNT1_BOTTOM (0xFFFF-0x0009)

ISR(SIG_OVERFLOW1) {
	unsigned char i;
	TCNT1 = TCNT1_BOTTOM;

	if(s_analog_pwm == 0){
	    if(s_analog_value[0] != 0) DEVICE_PORT_OUT0_(1); // verbose code for speed
	    if(s_analog_value[1] != 0) DEVICE_PORT_OUT1_(1);
	    if(s_analog_value[2] != 0) DEVICE_PORT_OUT2_(1);
	    if(s_analog_value[3] != 0) DEVICE_PORT_OUT3_(1);
	    if(s_analog_value[4] != 0) DEVICE_PORT_OUT4_(1);
	    if(s_analog_value[5] != 0) DEVICE_PORT_OUT5_(1);
	    if(s_analog_value[6] != 0) DEVICE_PORT_OUT6_(1);
	    if(s_analog_value[7] != 0) DEVICE_PORT_OUT7_(1);
	}else{
		if(s_analog_pwm == s_analog_value[0]) DEVICE_PORT_OUT0_(0); // verbose code for speed
	    if(s_analog_pwm == s_analog_value[1]) DEVICE_PORT_OUT1_(0);
	    if(s_analog_pwm == s_analog_value[2]) DEVICE_PORT_OUT2_(0);
	    if(s_analog_pwm == s_analog_value[3]) DEVICE_PORT_OUT3_(0);
	    if(s_analog_pwm == s_analog_value[4]) DEVICE_PORT_OUT4_(0);
	    if(s_analog_pwm == s_analog_value[5]) DEVICE_PORT_OUT5_(0);
	    if(s_analog_pwm == s_analog_value[6]) DEVICE_PORT_OUT6_(0);
	    if(s_analog_pwm == s_analog_value[7]) DEVICE_PORT_OUT7_(0);
    }
    //device_port_out(i, 1);
    //device_port_out(i, 0);
    
	s_analog_pwm++;
}

static void
analogWrite_global_method(JSVirtualMachine * vm, JSBuiltinInfo * builtin_info,
				  void *instance_context, JSNode * result_return, JSNode * args)
{
	JSInt32 pin;
	JSNode n;

	result_return->type = JS_BOOLEAN;
	result_return->u.vboolean = 0;

	if (args->u.vinteger == 2) {
		pin = js_vm_to_int32(vm, &args[1]);
		if(pin < 0 || 7 < pin) {
			return;
		}

		js_vm_to_number(vm, &args[2], &n);

		if(n.type == JS_INTEGER) {
			s_analog_value[pin] = n.u.vinteger;
		} else if (n.type == JS_FLOAT) {
			s_analog_value[pin] = (int)(n.u.vfloat * 0xff);
		} else {
			return;
		}

		result_return->u.vboolean = 1;
	}
}

/*

static void
analogWrite_global_method(JSVirtualMachine * vm, JSBuiltinInfo * builtin_info,
				  void *instance_context, JSNode * result_return, JSNode * args)
{
	unsigned char d;
	JSInt32 pin;
	JSNode n;

	result_return->type = JS_BOOLEAN;
	result_return->u.vboolean = 0;

	if (args->u.vinteger == 2) {
		pin = js_vm_to_int32(vm, &args[1]);
		js_vm_to_number(vm, &args[2], &n);

		if(n.type == JS_INTEGER) {
			d = n.u.vinteger;
		} else if (n.type == JS_FLOAT) {
			d = (int)(n.u.vfloat * 0xff);
		} else {
			return;
		}

		switch(pin) {
		case 0:
			if(! PWM_is_inited(0)) {
				PWM_init(0);
			}
			PWM_out(0,d);
			break;
		case 1:
			if(! PWM_is_inited(1)) {
				PWM_init(1);
			}
			PWM_out(1,d);
			break;
		default:
			return;
		}
		result_return->u.vboolean = 1;
	}
}
*/

static void
soundWrite_global_method(JSVirtualMachine * vm, JSBuiltinInfo * builtin_info,
				  void *instance_context, JSNode * result_return, JSNode * args)
{
	JSNode n;
	JSInt32 pin;
	JSFloat f;

	result_return->type = JS_BOOLEAN;
	result_return->u.vboolean = 0;

	if (args->u.vinteger == 2) {
		pin = js_vm_to_int32(vm, &args[1]);
		js_vm_to_number(vm, &args[2], &n);

		if(n.type == JS_INTEGER) {
			f = (JSFloat)n.u.vinteger;
		} else if (n.type == JS_FLOAT) {
			f = n.u.vfloat;
		} else {
			return;
		}

		if(f == 0) {
			switch(pin) {
			case 0:
				SOUND_off(1);
				break;
			default:
				return;
			}
		} else {
			if(f<245) {
				f = 245;
			}
			switch(args[1].u.vinteger) {
			case 0:
				if(! SOUND_is_inited(1)) {
					SOUND_init(1);
				}
				SOUND_out(1,f);
				break;
			default:
				return;
			}
		}
		result_return->u.vboolean = 1;
	}
}

void init_builtin_analogio(JSVirtualMachine *vm) {
	JSBuiltinInfo *info;
	JSNode *n;
	unsigned char i;

	struct {
		char *name;
		JSBuiltinGlobalMethod method;
	} global_methods[] = {
		{"analogRead", analogRead_global_method},
		{"analogWrite", analogWrite_global_method},
		{"soundWrite", soundWrite_global_method},
		{NULL, NULL}
	};

	for (i = 0; global_methods[i].name; i++) {
		info = js_vm_builtin_info_create(vm);
		info->global_method_proc = global_methods[i].method;

		n = &vm->globals[js_vm_intern(vm, global_methods[i].name)];
		js_vm_builtin_create(vm, n, info, NULL);
	}
	
	TCCR1A = 0;
	TCCR1B = 0x03; // 1/64 Clock
    TCNT1 = TCNT1_BOTTOM;
	TIMSK |= (1 << TOIE1); // Enable counter overflow interrupt
	sei(); // Enable global interrupt
	
}
