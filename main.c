#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef F_CPU
#define F_CPU 14745600
#endif /* F_CPU */

const unsigned char num[10] =
{
	~0x3F & 0x7F,
	~0x06 & 0x7F,
	~0x5B & 0x7F,
	~0x4F & 0x7F,
	~0x66 & 0x7F,
	~0x6D & 0x7F,
	~0x7D & 0x7F,
	~0x07 & 0x7F,
	~0x7F & 0x7F,
	~0x6F & 0x7F
};

volatile unsigned char segment[4] = {0xFF, 0xFF, 0xFF, 0xFF};

void isr_led_disp(void);
void set_num(unsigned char i, unsigned char n);
void set_dot(unsigned char i, unsigned char s);
void itobcd(unsigned short n, unsigned char *s);

int main(void)
{
	/* init timer, ctc mode, prescaler 64, 1 ms interrupt */
	TCCR0 = (1 << WGM01) | (1 << CS01) | (1 << CS00);
	OCR0 = 23;
	TIMSK = (1 << OCIE0);

	DDRA = 0x0F;
	DDRC = 0xFF; /* 11111111 */

	PORTA = (1 << PA4) | (1 << PA5) | (1 << PA6);

	set_dot(2, 1);

	set_num(0, 0);
	set_num(1, 0);
	set_num(2, 0);
	set_num(3, 0);

	asm volatile("sei");

	while(1)
	{

	}

	return 0;
}

void set_num(unsigned char i, unsigned char n)
{
	unsigned char p;
	p = i & 0x03;
	segment[p] &= 0x80;
	segment[p] |= num[n];
}

void set_dot(unsigned char i, unsigned char s)
{
	if(i < 4)
	{
		segment[i] = (s) ? (segment[i] & 0x7F) : (segment[i] | 0x80);
	}
	else
	{
		if(s)
		{
			PORTA &= ~(1 << PA4);
		}
		else
		{
			PORTA |= (1 << PA4);
		}
	}
}

void isr_led_disp(void)
{
	static unsigned char cur_seg = 0;

	PORTA &= ~(1 << cur_seg);

	if(++cur_seg == 4)
	{
		cur_seg = 0;
	}

	PORTC = segment[cur_seg];
	PORTA |= (1 << cur_seg);
}

void itobcd(unsigned short n, unsigned char *s)
{
	unsigned short a, mod = 1;
	unsigned char idx = 0, tmp = 0;

	do
	{
		mod *= 10;
		a = n % mod;
		n -= a;

		if(a == 0)
		{
			s[idx++] = 0;
			continue;
		}

		while(a % 10 == 0)
		{
			a /= 10;
		}

		s[idx++] = a;
	}
	while(n != 0 && idx < 4);

	for(; idx < 4; ++idx)
	{
		s[idx] = 0;
	}

	tmp = s[0];
	s[0] = s[3];
	s[3] = tmp;

	tmp = s[1];
	s[1] = s[2];
	s[2] = tmp;
}

ISR(TIMER0_COMP_vect)
{
	static unsigned char val[4];
	static unsigned short hms = 0, cnt = 0, fct = 0, sbd = 0;
	static unsigned char btn_reset = 0, btn_cont = 0, btn_touch = 0,
	cms = 0, running = 0, flash = 0, sb = 0;
	static unsigned short touch = 0;
	unsigned char i;

	if(sb)
	{
		if(++sbd == 10000)
		{
			itobcd(touch, val);
			set_dot(2, 0);
			for(i = 0; i < 4; ++i)
			{
				set_num(i, val[i]);
			}			
		}
		else if(sbd == 20000)
		{
			sbd = 0;
			itobcd(hms, val);
			set_dot(2, 1);
			for(i = 0; i < 4; ++i)
			{
				set_num(i, val[i]);
			}
		}
	}
	else
	{
		/* 100 ms */
		if(running && (++cnt == 1000))
		{
			if(hms < 9999)
			{
				++hms;

				if(!flash)
				{
					itobcd(hms, val);
					for(i = 0; i < 4; ++i)
					{
						set_num(i, val[i]);
					}
				}
			}

			cnt = 0;
		}

		if(flash && (++fct == 3500))
		{
			fct = 0;
			if((flash & 1) == 0)
			{
				for(i = 0; i < 4; ++i)
				{
					set_num(i, val[i]);
				}
			}
			else
			{
				for(i = 0; i < 4; ++i)
				{
					segment[i] = 0xFF;
				}
			}

			if(--flash == 0)
			{
				set_dot(2, 1);
			}
		}
	}

	/* 1 ms */
	if(++cms == 10)
	{
		cms = 0;
		/* debouncing */
		/* wire touch */
		if(PINA & (1 << PA4))
		{
			btn_touch = 0;
		}
		else if(btn_touch < 50)
		{
			++btn_touch;
		}
		else if(btn_touch == 50)
		{
			btn_touch = 0xFF;
			if(touch < 9999) ++touch;
			flash = 7;
			fct = 0;
			itobcd(touch, val);
		}

		/* reset */
		if(PINA & (1 << PA5))
		{
			btn_reset = 0;
		}
		else if(btn_reset < 50)
		{
			++btn_reset;
		}
		else if(btn_reset == 50)
		{
			btn_reset = 0xFF;
			running = 0;
			sbd = 0;
			sb = 1;
		}

		/* continue */
		if(PINA & (1 << PA6))
		{
			btn_cont = 0;
		}
		else if(btn_cont < 50)
		{
			++btn_cont;
		}
		else if(btn_cont == 50)
		{
			btn_cont = 0xFF;
			if(sb)
			{
				sb = 0;
				hms = 0;
				touch = 0;
			}
			running = !running;
		}
	}

	isr_led_disp();
}
