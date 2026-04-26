#define F_CPU 8000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define USART_BAUDRATE 9600
#define USART_BAUDRATE_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)
#define BITRATE(TWSR) ((F_CPU/100000)-16)/(2*pow(4,(TWSR&((1<<TWPS0)|(1<<TWPS1)))))
#define WRITE_SLAVE_ADDRESS 0xD0
#define READ_SLAVE_ADDRESS 0xD1
//#define RTC_EEPROM_MAGIC 0xA5U
//static uint8_t EEMEM rtc_eeprom_init_flag = 0xFF;

#define LCD_Dir DDRA
#define LCD_Port PORTA
#define RS PA0
#define EN PA1

#define KPD_DDR DDRB
#define KPD_PORT PORTB
#define KPD_PIN PINB

#define pb1 PD2
#define pb2 PD7
#define trig_pin PD5
#define echo_pin PD6

#define Clock_Pin PC2
#define Latch_Pin PC3
#define Data_Input_Pin PC4

#define Clock_Pin2 PC5
#define Latch_Pin2 PC6
#define Data_Input_Pin2 PC7

#define LDR_Pin PA2
#define FLAME_Pin PA3
#define Servo_Motor PD4


unsigned char keypad[4][4] = {{'7','8','9','/'},
	                          {'4','5','6','*'},
							  {'1','2','3','-'},
							  {'C','0','=','+'}};
								  
unsigned char rows , cols ;
char key[6] , user_password[7] ;
char password[] = "123456" ;
volatile int pb1state = 0 ;
int i , d , d1 , LDR_Val , sun_light , Flame_Val , count = 1 , pb1newval , pb1oldval = 1 , pb2newval , pb2oldval = 1 , pb2state = 1 ;
unsigned int b , b1 ;
uint16_t T1 , T2 , D ;
float echoTravelTime , echoTargetTime , echoTravelDistance , echoTargetDistance , TEMP ;
char buffer1[50] , buffer2[50] , buffer3[50] , buffer4[50] , buffer5[70] , buffer6[50] , buffer7[50] , buffer8[15] , buffer9[30] ;
unsigned char hour , minute , second , day , month , year ;



void LCD_Command(unsigned char cmnd){
	
	LCD_Port = (LCD_Port & 0x0F) | (cmnd & 0xF0);
	LCD_Port &= ~(1 << RS);
	LCD_Port |= (1 << EN);
	_delay_us(2);
	LCD_Port &= ~(1 << EN);
	
	_delay_us(250);
	
	LCD_Port = (LCD_Port & 0x0F) | (cmnd << 4);
	LCD_Port |= (1 << EN);
	_delay_us(2);
	LCD_Port &= ~(1 << EN);
	
	_delay_ms(7);
	
}

void LCD_Char(unsigned char data){
	
	LCD_Port = (LCD_Port & 0x0F) | (data & 0xF0);
	LCD_Port |= (1 << RS);
	LCD_Port |= (1 << EN);
	_delay_us(2);
	LCD_Port &= ~(1 << EN);
	
	_delay_us(250);
	
	LCD_Port = (LCD_Port & 0x0F) | (data << 4);
	LCD_Port |= (1 << EN);
	_delay_us(2);
	LCD_Port &= ~(1 << EN);
	
	_delay_ms(7);
	
}

void LCD_init(){
	
	//LCD_Dir = 0xFF ;
	LCD_Dir |= (1 << RS) | (1 << EN) | (1 << PA4) | (1 << PA5) | (1 << PA6) | (1 << PA7);
	_delay_ms(20);
	
	LCD_Command(0x33);
	LCD_Command(0x32);
	LCD_Command(0x28);
	LCD_Command(0x0c);
	LCD_Command(0x06);
	LCD_Command(0x01);
	//LCD_Command(0x08);
	//LCD_Command(0x0E);
	//LCD_Command(0x0F);
	//LCD_Command(0x80);
	//LCD_Command(0xC0);
	
}

void LCD_String(char *str){
	
	int i;
	for(i = 0 ; str[i] != 0 ; i++){
		
		LCD_Char(str[i]);
		
	}
	
}

void LCD_String_xy(char row,char col,char *str){
	
	if(row == 0 && col < 16){
		LCD_Command((col & 0x0F) | 0x80);
	}
	else if(row == 1 && col < 16){
		LCD_Command((col & 0x0F) | 0xC0);
	}
	
	LCD_String(str);
	
}

void LCD_Double(double val,int width,int precision){
	
	char d_buffer[30] ;
	dtostrf(val,width,precision,d_buffer);
	LCD_String(d_buffer);
}

void LCD_Integer(int val){
	
	char I_buffer[30] ;
	itoa(val,I_buffer,10);
	LCD_String(I_buffer);
}

void LCD_Clear(){
	
	LCD_Command(0x01);
	_delay_us(2);
	LCD_Command(0x80);
	
}


void USART_init(long BAUDRATE){
	
	UCSRB |= (1 << RXEN) | (1 << TXEN);
	UCSRC |= (1 << URSEL) | (1 << UCSZ0) | (1 << UCSZ1);
	UBRRL = USART_BAUDRATE_PRESCALE ;
	UBRRH &= ~(1 << URSEL);
	UBRRH = (USART_BAUDRATE_PRESCALE >> 8);
	
}

unsigned char USART_RxChar(){
	
	while((UCSRA & (1 << RXC)) == 0){
		
	}
	
	return UDR ;
}

void USART_TxChar(char data){
	
	while((UCSRA & (1 << UDRE)) == 0){
		
	}
	
	UDR = data ;
}

void USART_String(char *str){
	
	unsigned char j ;
	for(j = 0 ; str[j] != 0 ; j++){
		
		USART_TxChar(str[j]);
		
	}
	
}

void HC595_init(){
	
	DDRC |= (1 << Clock_Pin) | (1 << Latch_Pin) | (1 << Data_Input_Pin);
	
}

void HC595_shiftOut(unsigned int data){
	
	PORTC &= ~(1 << Latch_Pin);
	
	for(d = 7 ; d >= 0 ; d--){  //LSB
		
		PORTC &= ~(1 << Clock_Pin);
		
		if((data & (1 << d)) == 0){
			
			PORTC &= ~(1 << Data_Input_Pin);
			
		}
		else{  //if((data & (1 << d)) != 0){}
			
			PORTC |= (1 << Data_Input_Pin);
			
		}
		
		PORTC |= (1 << Clock_Pin);
	}
	
	PORTC |= (1 << Latch_Pin);
	
}

void HC74_595_init(){
	
	DDRC |= (1 << Clock_Pin2) | (1 << Latch_Pin2) | (1 << Data_Input_Pin2);
	
}

void HC74_shiftOut(unsigned int data1){
	
	PORTC &= ~(1 << Latch_Pin2);
	
	for(d1 = 0 ; d1 <= 7 ; d1++){  //MSB
		
		PORTC &= ~(1 << Clock_Pin2);
		
		if((data1 & (1 << d1)) == 0){
			
			PORTC &= ~(1 << Data_Input_Pin2);
			
		}
		else{  //if((data1 & (1 << d1)) != 0){}
			
			PORTC |= (1 << Data_Input_Pin2);
			
		}
		
		PORTC |= (1 << Clock_Pin2);
	}
	
	PORTC |= (1 << Latch_Pin2);
	
}


void ADC_init(){
	
	DDRA &= ~(1 << LDR_Pin);
	ADMUX = 0x42 ;
	ADCSRA = 0x87 ;
	
}

int ADC_LDR_Read(char channel){
	
	ADMUX = ADMUX | (channel & 0x0F);
	ADCSRA |= (1 << ADSC);
	while((ADCSRA & (1 << ADIF)) == 0){
		
	}
	ADCSRA |= (1 << ADIF);
	_delay_ms(1);
	
	 return ADCW ;
}

/*
void ADC_FLAME_init(){
	
	DDRA &= ~(1 << FLAME_Pin);
	ADMUX = 0x43 ;
	ADCSRA = 0x87 ;
}


int ADC_FLAME_Read(char channel){
	
	ADMUX = ADMUX | (channel & 0x0F);
	ADCSRA |= (1 << ADSC);
	while((ADCSRA & (1 << ADIF)) == 0){
		
	}
	ADCSRA |= (1 << ADIF);
	_delay_ms(1);
	
	return ADCW ;
}
*/

void TWI_init(){
	
	TWBR = BITRATE(TWSR = 0x00);
	
}

unsigned int TWI_START(char WRITE_ADDRESS){
	
	unsigned int status ;
	TWCR = (1 << TWEN) | (1 << TWINT) | (1 << TWSTA);
	while((TWCR & (1 << TWINT)) == 0){
		
	}
	status = TWSR & 0xF8 ;
	if(status != 0x08){
		return 0 ;
	}
	TWDR = WRITE_ADDRESS ;
	TWCR = (1 << TWEN) | (1 << TWINT);
	while((TWCR & (1 << TWINT)) == 0){
		
	}
	status = TWSR & 0xF8 ;
	if(status == 0x18){
		return 1 ;
	}
	if(status == 0x20){
		return 2 ;
	}
	else{
		return 3 ;
	}
	
}

unsigned int TWI_REPEATED_START(char READ_ADDRESS){
	
	unsigned int status ;
	TWCR = (1 << TWEN) | (1 << TWINT) | (1 << TWSTA);
	while((TWCR & (1 << TWINT)) == 0){
		
	}
	status = TWSR & 0xF8 ;
	if(status != 0x10){
		return 0 ;
	}
	TWDR = READ_ADDRESS ;
	TWCR = (1 << TWEN) | (1 << TWINT);
	while((TWCR & (1 << TWINT)) == 0){
		
	}
	status = TWSR & 0xF8 ;
	if(status == 0x40){
		return 1 ;
	}
	if(status == 0x48){
		return 2 ;
	}
	else{
		return 3 ;
	}
	
}

unsigned int TWI_WRITE(char data){
	
	unsigned int status ;
	TWDR = data ;
	TWCR = (1 << TWEN) | (1 << TWINT);
	while((TWCR & (1 << TWINT)) == 0){
		
	}
	status = TWSR & 0xF8 ;
	if(status == 0x28){
		return 0 ;
	}
	if(status == 0x30){
		return 1 ;
	}
	else{
		return 2 ;
	}
	
}

char TWI_READ_ACK(){
	
	TWCR = (1 << TWEN) | (1 << TWINT) | (1 << TWEA);
	while((TWCR & (1 << TWINT)) == 0){
		
	}
	
	return TWDR ;
}

char TWI_READ_NACK(){
	
	TWCR = (1 << TWEN) | (1 << TWINT);
	while((TWCR & (1 << TWINT)) == 0){
		
	}
	
	return TWDR ;
}

void TWI_STOP(){
	
	TWCR = (1 << TWEN) | (1 << TWINT) | (1 << TWSTO);
	while((TWCR & (1 << TWSTO))){
		
	}
	
}

unsigned char DEC_TO_BCD(unsigned char val){
	
	unsigned char result ;
	result = (val / 10 * 16) + (val % 10);
	
	return result ;
}

unsigned char BCD_TO_DEC(unsigned char val){
	
	unsigned char result ;
	result = (val / 16 * 10) + (val % 16);
	
	return result ;
}

void SET_TIME(unsigned char h , unsigned char m , unsigned char s , unsigned char dom , unsigned char mo , unsigned char y){
	
	TWI_START(WRITE_SLAVE_ADDRESS);
	TWI_WRITE(0x00);
	TWI_WRITE(DEC_TO_BCD(s));
	TWI_WRITE(DEC_TO_BCD(m));
	TWI_WRITE(DEC_TO_BCD(h));
	TWI_WRITE(0x01);
	TWI_WRITE(DEC_TO_BCD(dom));
	TWI_WRITE(DEC_TO_BCD(mo));
	TWI_WRITE(DEC_TO_BCD(y));
	TWI_STOP();
	
}

void GET_TIME(unsigned char *h , unsigned char *m , unsigned char *s , unsigned char *dom , unsigned char *mo , unsigned char *y){
	
	TWI_START(WRITE_SLAVE_ADDRESS);
	TWI_WRITE(0x00);
	TWI_REPEATED_START(READ_SLAVE_ADDRESS);
	*s = BCD_TO_DEC(TWI_READ_ACK());
	*m = BCD_TO_DEC(TWI_READ_ACK());
	*h = BCD_TO_DEC(TWI_READ_ACK());
	TWI_READ_ACK();
	*dom = BCD_TO_DEC(TWI_READ_ACK());
	*mo = BCD_TO_DEC(TWI_READ_ACK());
	*y = BCD_TO_DEC(TWI_READ_NACK());
	TWI_STOP();
	
	
}

unsigned char Get_month(char *m){
	
	if(strcmp(m,"Jan") == 0){
		return 1 ;
	}
	if(strcmp(m,"Feb") == 0){
		return 2 ;
	}
	if(strcmp(m,"Mar") == 0){
		return 3 ;
	}
	if(strcmp(m,"Apr") == 0){
		return 4 ;
	}
	if(strcmp(m,"May") == 0){
		return 5 ;
	}
	if(strcmp(m,"Jun") == 0){
		return 6 ;
	}
	if(strcmp(m,"Jul") == 0){
		return 7 ;
	}
	if(strcmp(m,"Aug") == 0){
		return 8 ;
	}
	if(strcmp(m,"Sep") == 0){
		return 9 ;
	}
	if(strcmp(m,"Oct") == 0){
		return 10 ;
	}
	if(strcmp(m,"Nov") == 0){
		return 11 ;
	}
	//if(strcmp(m,"Dec") == 0){
	return 12 ;
}

float GET_TEMPERATURE(){
	
	int8_t msb ;
	uint8_t lsb ;
	float temp ;
	
	TWI_START(WRITE_SLAVE_ADDRESS);
	TWI_WRITE(0x11);
	TWI_REPEATED_START(READ_SLAVE_ADDRESS);
	msb = TWI_READ_ACK();
	lsb = TWI_READ_NACK();
	TWI_STOP();
	temp = (msb + (lsb >> 6) * 0.25) ;
	
	return temp;
}

/*
void RTC_Init_Once_From_Build(){
	
	unsigned char h, m, s, dom, mo, y;
	char tbuf[] = __TIME__;
	char dbuf[] = __DATE__;
	char *tok;
	
	if(eeprom_read_byte(&rtc_eeprom_init_flag) == RTC_EEPROM_MAGIC){
		return;
	}
	
	tok = strtok(tbuf, ":");
	h = (unsigned char)atoi(tok);
	m = (unsigned char)atoi(strtok(NULL, ":"));
	s = (unsigned char)atoi(strtok(NULL, ":"));
	
	mo = Get_month(strtok(dbuf, " "));
	dom = (unsigned char)atoi(strtok(NULL, " "));
	y = (unsigned char)(atoi(strtok(NULL, " ")) % 100);
	
	SET_TIME(h, m, s, dom, mo, y);
	eeprom_write_byte(&rtc_eeprom_init_flag, RTC_EEPROM_MAGIC);
}
*/

char GetKey(){
	
	while(1){
		
		KPD_DDR = 0xF0 ;
		KPD_PORT = 0x0F ;
		
		//We first check if there is any button being pressed or not
		do{
			
			KPD_PORT = 0x0F ;
			asm("NOP");
			cols = (KPD_PIN & 0x0F);
			
		}while(cols != 0x0F);
		
		//Then we start reading the cols
		do{
			do{
				
				pb2newval = (PIND & (1 << pb2));
				if(pb2oldval == 0 && pb2newval != 0){
					if(pb2state == 1){
						pb2state = 2 ;
					}
					else{  //if(pb2state == 2){}
						pb2state = 1 ;
					}
				}
				pb2oldval = pb2newval ;
				_delay_ms(20);
				
				_delay_ms(20);
				cols = (KPD_PIN & 0x0F);
				
			}while(cols == 0x0F);
			
			//Then we double check it
			_delay_ms(40);
			cols = (KPD_PIN & 0x0F);
			
		}while(cols == 0x0F);
		
		//Then we start the scanning process
		//We start with the 1st row
		KPD_PORT = 0xEF ;
		asm("NOP");
		cols = (KPD_PIN & 0x0F);
		if(cols != 0x0F){			
			rows = 0 ;
			break ;
		}
		
		//Then we scan the 2nd row
		KPD_PORT = 0xDF ;
		asm("NOP");
		cols = (KPD_PIN & 0x0F);
		if(cols != 0x0F){			
			rows = 1 ;
			break ;
		}
		
		//Then we scan the 3rd row
		KPD_PORT = 0xBF ;
		asm("NOP");
		cols = (KPD_PIN & 0x0F);
		if(cols != 0x0F){			
			rows = 2 ;
			break ;
		}
		
		//Then we scan the 4th row
		KPD_PORT = 0x7F ;
		asm("NOP");
		cols = (KPD_PIN & 0x0F);
		if(cols != 0x0F){		
			rows = 3 ;
			break ;
		}
	
	}
	
	if(cols == 0x0E){
		return(keypad[rows][0]);
	}
	else if(cols == 0x0D){
		return(keypad[rows][1]);
	}
	else if(cols == 0x0B){
		return(keypad[rows][2]);
	}
	else{  //if(cols == 0x07){}
		return(keypad[rows][3]);
	}
	
}

ISR(INT0_vect){
	
	if(pb1state == 0){
		pb1state = 1 ;
	}
	else if(pb1state == 1){
		pb1state = 2 ;
	}
	else if(pb1state == 2){
		pb1state = 3 ;
	}
	else if(pb1state == 3){
		pb1state = 4 ;
	}
	else if(pb1state == 4){
		pb1state = 5 ;
	}
	else if(pb1state == 5){
		pb1state = 6 ;
	}
	else{  //if(pb1state == 6){}
	pb1state = 0 ;
    }
	
}

ISR(INT1_vect){
	
	GICR &= ~(1 << INT1);
	
	if(OCR1B == 65){
		OCR1B = 300 ;
	}
	else{
		OCR1B = 65 ;
	}
	
}


int main(){
	
	DDRD &= ~((1 << pb1) | (1 << pb2));
	PORTD |= (1 << pb1) | (1 << pb2);
	LCD_init();
	USART_init(USART_BAUDRATE);
	HC595_init();
	ADC_init();
	HC74_595_init();
	TWI_init();
	//ADC_FLAME_init();
	//RTC_Init_Once_From_Build();
	
	while(1){
		
		GICR = (1 << INT0);
		MCUCR = (1 << ISC01) | (1 << ISC00);
		sei();
		
		/*
		pb1newval = (PIND & (1 << pb1));
		if(pb1oldval == 0 && pb1newval == 1){
			if(pb1state == 0){
				pb1state = 1 ;
			}
			else if(pb1state == 1){
				pb1state = 2 ;
			}
			else if(pb1state == 2){
				pb1state = 3 ;
			}
			else if(pb1state == 3){
				pb1state = 4 ;
			}
			else if(pb1state == 4){
				pb1state = 5 ;
			}
			else if(pb1state == 5){ 
				pb1state = 6 ;
			}
			else{  //if(pb1state == 6){}
				pb1state = 0 ;
			}			
		}
		
		pb1oldval = pb1newval ;
		_delay_ms(50);
		*/
		if(pb1state == 0){
			
			LCD_Command(0x01);
			do{
				
				LCD_Command(0x80);
				LCD_String("Press Pb1 to");
				LCD_Command(0xC0);
				LCD_String("start...");
				_delay_ms(1000);
				LCD_Clear();
				LCD_Command(0x80);
				LCD_String("1.PASS  2.MOTION");
				LCD_Command(0xC0);
				LCD_String("3.FIRE  4.RAIN");
				_delay_ms(1000);
				LCD_Clear();
				LCD_Command(0x80);
				LCD_String("5.SUN.L  6.TEMP");
				_delay_ms(1000);
				LCD_Command(0x01);
				
				GICR = (1 << INT0);
				MCUCR = (1 << ISC01) | (1 << ISC00);
				sei();
				/*
				pb1newval = (PIND & (1 << pb1));
				if(pb1oldval == 0 && pb1newval == 1){
					if(pb1state == 0){
						pb1state = 1 ;
					}
					else if(pb1state == 1){
						pb1state = 2 ;
					}
					else if(pb1state == 2){
						pb1state = 3 ;
					}
					else if(pb1state == 3){
						pb1state = 4 ;
					}
					else if(pb1state == 4){
						pb1state = 5 ;
					}
					else if(pb1state == 5){
						pb1state = 6 ;
					}
					else{  //if(pb1state == 6){}
					pb1state = 0 ;
					}
				}
				
				pb1oldval = pb1newval ;
				_delay_ms(50);
				*/
			}while(pb1state == 0);
			
		}
		
		if(pb1state == 1){
			
			count = 1 ;
			
			while(count < 4){
				
				LCD_Command(0x01);
				LCD_Command(0x80);
				LCD_String("Enter Password");
				LCD_Command(0xC0);
				
				for(i = 0 ; i < 6 ; i++){
					
					key[i] = GetKey();
					
					if(pb2state == 1){
						
						user_password[i] = key[i] ;
						LCD_Char(user_password[i]);
						
					}
					else if(pb2state == 2){
						
						user_password[i] = key[i] ;
						LCD_Char('*');
						
					}
					
				}				
				user_password[6] = '\0' ;
				
				if(strcmp(user_password,password) == 0){
					
					LCD_Command(0x01);
					LCD_Command(0x80);
					LCD_String("CORRECT PASSWORD");
					LCD_Command(0xC0);
					LCD_String("ACCESS GRANTED!");
					_delay_ms(2000);
					break ;
					
				}
				else{
					
					count++ ;
					LCD_Command(0x01);
					LCD_Command(0x80);
					LCD_String("WRONG PASSWORD");
					LCD_Command(0xC0);
					LCD_String("ACCESS DENIED!");
					_delay_ms(2000);
					
				}
			}
			if(count > 3){
					
				LCD_Command(0x01);
				LCD_Command(0x80);
				LCD_String("ATTEMPTS FAILED!");
				LCD_Command(0xC0);
				LCD_String("SYSTEM LOCKED!");
				_delay_ms(4000);
				count = 1 ;
			}
			else{
					
				LCD_Command(0x01);
				LCD_Command(0x80);
				LCD_String("LOGGED IN...");
				_delay_ms(4000);
				count = 9 ;
				pb1state = 0 ;
				//break ;
			}

		}
		
		if(pb1state == 2){
			
			LCD_Command(0x01);
			
			DDRD |= (1 << trig_pin);
			DDRD &= ~(1 << echo_pin);
			TCCR1A = 0x00 ;
			TCCR1B |= (1 << ICNC1);
			
			while(pb1state == 2){
				
				GICR = (1 << INT0);
				MCUCR = (1 << ISC01) | (1 << ISC00);
				sei();
				
				PORTD &= ~(1 << trig_pin);
				_delay_us(10);
				PORTD |= (1 << trig_pin);
				_delay_us(10);
				PORTD &= ~(1 << trig_pin);
				
				TCNT1 = 0x00 ;
				TIFR |= (1 << ICF1);
				
				TCCR1B |= (1 << ICES1);
				TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));
				TCCR1B |= (1 << CS11);
				while((TIFR & (1 << ICF1)) == 0){
					
				}
				T1 = ICR1 ;
				TIFR |= (1 << ICF1);
				
				TCCR1B &= ~(1 << ICES1);
				TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));
				TCCR1B |= (1 << CS11);
				while((TIFR & (1 << ICF1)) == 0){
					
				}
				T2 = ICR1 ;
				
				//D = ((1/(Fosc/prescalar)) * 10^6) * (C) :-  D = ((1/(Fosc/prescalar)) * 10^6) * (T2 - T1) :-  D = ((1/(8000000/8)) * 10^6) * (T2 - T1)
				D = 1 * (T2 - T1);
				echoTravelTime = (float)D;
				echoTargetTime = echoTravelTime / 2.0 ;
				echoTravelDistance = 0.0343 * echoTravelTime ;
				echoTargetDistance = echoTravelDistance / 2.0 ;
				
				LCD_Command(0x80);
				LCD_String("ETD : ");
				LCD_Double(echoTargetDistance,5,2);
				LCD_String(" cm");
				LCD_String("      ");
				LCD_Command(0xC0);
				LCD_String("MOTION: ");
				
				sprintf(buffer1,"echoTravelTime : %.2f Ms",echoTravelTime);
				sprintf(buffer2,"echoTargetTime : %.2f Ms",echoTargetTime);
				sprintf(buffer3,"echoTravelDist : %.2f cm",echoTravelDistance);
				USART_String(buffer1);
				USART_String("\r\n");
				USART_String(buffer2);
				USART_String("\r\n");
				USART_String(buffer3);
				USART_String("\r\r\n\n");
				
				if(echoTargetDistance > 1100.00){
					
					LCD_String("NEGATIVE");
					LCD_String("        ");
					HC595_shiftOut(0b00000000);
				}
				else if(echoTargetDistance <= 1100.00 && echoTargetDistance > 825.00){
					
					LCD_String("POSITIVE");
					LCD_String("        ");
					HC595_shiftOut(0b11000000);
					
				}
				else if(echoTargetDistance <= 825.00 && echoTargetDistance > 550.00){
					
					LCD_String("PODITIVE");
					LCD_String("        ");
					HC595_shiftOut(0b00110000);
					
				}
				else if(echoTargetDistance <= 550.00 && echoTargetDistance > 275.00){
					
					LCD_String("POSITIVE");
					LCD_String("        ");
					HC595_shiftOut(0b00001100);
					
				}
				else if(echoTargetDistance <= 275.00){
					
					LCD_String("POSITIVE");
					LCD_String("        ");
					HC595_shiftOut(0b00000011);
					
				}
				
			}
				
		}
		
		if(pb1state == 3){
			
			LCD_Command(0x01);
			
			while(pb1state == 3){
				
				GICR = (1 << INT0);
				MCUCR = (1 << ISC01) | (1 << ISC00);
				sei();
				
			    LDR_Val = (ADC_LDR_Read(2) / 4);
				//sun_light = ((100/241) * LDR_Val);
				//sun_light = (((100 * LDR_Val) / 241) - 4);
				sun_light = ((241 - LDR_Val) * 100) / (241 - 12);
				if(sun_light < 0){
					sun_light = 0 ;
				}

			    LCD_Command(0x80);
			    LCD_String("SUN-L. : ");
			    LCD_Integer(sun_light);
				LCD_String("%");
			    LCD_String("        ");
				
				sprintf(buffer4,"LDR Val : %d",LDR_Val);
				USART_String(buffer4);
				USART_String("\r\n");
				
				if(sun_light == 100){
						
					HC74_shiftOut(0b01100000);
					LCD_Command(0xC0);
					LCD_String("MOTOR : ");
					LCD_String("ON");
					LCD_String("        ");
				}
				else if(sun_light < 100 && sun_light > 80){
					
					HC74_shiftOut(0b01100001);
					LCD_Command(0xC0);
					LCD_String("MOTOR : ");
					LCD_String("ON");
					LCD_String("        ");		
				}
				else if(sun_light <= 80 && sun_light > 65){
					
					HC74_shiftOut(0b01100011);
					LCD_Command(0xC0);
					LCD_String("MOTOR : ");
					LCD_String("ON");
					LCD_String("        ");	
				}
				else if(sun_light <= 65 && sun_light > 45){
					
					HC74_shiftOut(0b01100111);
					LCD_Command(0xC0);
					LCD_String("MOTOR : ");
					LCD_String("ON");
					LCD_String("        ");
				}
				else if(sun_light <= 45 && sun_light > 20){
					
					HC74_shiftOut(0b00001111);
					LCD_Command(0xC0);
					LCD_String("MOTOR : ");
					LCD_String("OFF");
					LCD_String("        ");
				}
				else if(sun_light <= 20 && sun_light >= 0){
					
					HC74_shiftOut(0b00011111);
					LCD_Command(0xC0);
					LCD_String("MOTOR : ");
					LCD_String("OFF");
					LCD_String("        ");
				}
				
			}
			
		}
		
		if(pb1state == 4){
			
			LCD_Command(0x01);
			USART_String("\r\n");
			
			char c_time[] = __TIME__ ;
			hour = atoi(strtok(c_time, ":"));
			minute = atoi(strtok(NULL, ":"));
			second = atoi(strtok(NULL, ":"));
			
			char c_date[] = __DATE__ ;
			month = Get_month(strtok(c_date, " "));
			day = atoi(strtok(NULL, " "));
			year = atoi(strtok(NULL, " ")) % 100;
			
			SET_TIME(hour,minute,second,day,month,year);
			
			while(pb1state == 4){
				
				GICR = (1 << INT0);
				MCUCR = (1 << ISC01) | (1 << ISC00);
				sei();			
				
				//RTC_Init_Once_From_Build();
				GET_TIME(&hour,&minute,&second,&day,&month,&year);
				TEMP = GET_TEMPERATURE();

				sprintf(buffer5,"%02d:%02d:%02d",hour,minute,second);
				sprintf(buffer6,"%02d:%02d:%02d",day,month,year);
				sprintf(buffer7,"TEMPERATURE : %.1f C",TEMP);
				
				LCD_Command(0x80);
				LCD_String("TIME: ");
				LCD_String(buffer5);
				LCD_Command(0xC0);
				LCD_String("DATE: ");
				LCD_String(buffer6);
				USART_String(buffer7);
				USART_String("\r\n");
				
				_delay_ms(50);
				
			}
					
		}
		
		if(pb1state == 5){
			
			DDRA &= ~(1 << FLAME_Pin);
			LCD_Command(0x01);
			USART_String("\r\n");
			while(pb1state == 5){
				
				GICR = (1 << INT0);
				MCUCR = (1 << ISC01) | (1 << ISC00);
				sei();
				
				Flame_Val = (PINA & (1 << FLAME_Pin));
				
				if(Flame_Val == 0){
					
					HC74_shiftOut(0b00000000);
					
					LCD_Command(0x80);
					LCD_String("FIRE : ");
					LCD_String("NEGATIVE");
					LCD_Command(0xC0);
					LCD_String("S.M.| MOTOR: OFF");
					LCD_String("                 ");
				}
				else{
					
					HC74_shiftOut(0b10100000);
					
					LCD_Command(0x80);
					LCD_String("FIRE : ");
					LCD_String("POSITIVE");
					LCD_Command(0xC0);
					LCD_String("D.M.| MOTOR: ON");
					LCD_String("                 ");					
				}
				
				sprintf(buffer8,"FLAME SENSOR : %d",Flame_Val);
				USART_String(buffer8);
				USART_String("\r\n");
							
			}
			
		}
		
		if(pb1state == 6){
			
			DDRD |= (1 << Servo_Motor);
			DDRD &= ~(1 << PD3);
			
			TCNT1 = 0x00 ;
			ICR1 = 2499 ;
			TCCR1A = (1 << WGM11) | (1 << COM1B1);
			TCCR1B = (1 << WGM12) | (1 << WGM13) | (1 << CS10) | (1 << CS11);
			OCR1B = 65 ;
			
			LCD_Command(0x01);
			USART_String("\r\n");
			
			while(pb1state == 6){
				
				GICR = (1 << INT0) | (1 << INT1);
				MCUCR |= (1 << ISC01) | (1 << ISC00) | (1 << ISC10);
				MCUCR &= ~(1 << ISC11);
				sei();
				
				if(OCR1B == 65){
					
					LCD_Command(0x80);
					LCD_String("RAIN: NEGATIVE");
					LCD_String("              ");
					LCD_Command(0xC0);
					LCD_String("WINDOWS OPEN");
					LCD_String("            ");
				}
				else{
					
					LCD_Command(0x80);
					LCD_String("RAIN: POSITIVE");
					LCD_String("              ");
					LCD_Command(0xC0);
					LCD_String("WINDOWS CLOSED");
					LCD_String("              ");
				}
				
				sprintf(buffer9,"OCR1B : %d",OCR1B);
				USART_String(buffer9);
				USART_String("\r\n");
			}
			
		}
		
		
		
		
	}
	

  
	return 0 ;
}






