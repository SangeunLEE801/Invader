#include <ucos_ii.h>
#include <os_cpu.h>
#include <stdlib.h>
#include <time.h>

//Add necessary header file for our platform
#include <stm32f4xx.h>
#include "LED.h"
#include "GLCD.h"
#include "I2C.h"
#include "JOY.h"

#define winX 3  //창의 시작 위치
#define winY 2  //창의 시작 위치
#define winWidth 20 //창의 폭
#define winHeight 36 //창의 높이

#define TAMPERBUT ((GPIOC->IDR & (1 << 13))==0)
#define USERBUT ((GPIOG->IDR & (1 << 15))==0)
#define WAKEUPBUT ((GPIOA->IDR & (1 << 0))==1)

#define INITIAL -1
#define BUT_DOWN 0
#define BUT_UP 1
#define TH 10

#define ESC 27 //게임 탈출 
#define SPACE 32 //총알, 점프 
#define LEFT 75 //224 다음에 75
#define RIGHT 77 //224 다음에 77
#define UP 72 //224 다음에 72
#define DOWN 80 //224 다음에 80

#define MAXBULLET 10
#define MAXENEMY 3
#define MAXENEMYBULLET 12

#define TRUE 1
#define FALSE 0

#define random(n) (rand()%(n))   //0~n까지의 난수 발생


extern OS_EVENT* tamperbut_sem;
extern OS_EVENT* done_sem;

struct {
	//BOOL exist;
	int exist;
	int x, y;
}Bullet[MAXBULLET];

struct {
	//BOOL exist;
	int exist;
	int type;
	int x, y;
	int dest;
	int move_count;
	int nFrame;
	int nStay;
	int nBull; //총알을 쏘는 간격 조절을 위한 변수
}Enemy[MAXENEMY];

struct {
	//BOOL exist;
	int exist;
	int x, y;
	int nFrame;
	int nStay;
}EnemyBullet[MAXENEMYBULLET];

extern struct {
	//BOOL exist;
	int exist;
	int x, y;
	int nFrame;
	int nStay;
}EnemyBulletLv2;


char *arrEnemy[] = { "ENE", "EN@" , "ENE", "ENY" };
char score[20];
char level[30];



int px = 10, py=39;
int bx = -1, by;
int mx = -1, my;

int Level = 1;
int Score = 0;
int GameOver = 0;
int life = 3;
int count=0;	//디버깅용
int led=6;
int is_collision=FALSE; //충돌시 무적
int grace_period=0; //무적시간


/*Function Prototypes*/
//void SetColor(int coloer);
void HeightLine(void);
void DrawPlayer(void);
void DrawPlayer_grace(void);
void ErasePlayer(void);
void PlayerMoveAction(void);
void DrawBullet(int i);
void EraseBullet(int i);
void PlayerBulletAction(void);
void DrawMissile(void);
void DrawEnemy(void);
void EnemyMoveAction(void);
void EnemyBulletAction(int i);
void DrawEnemyBullet(int i);
void EraseEnemyBullet(int i);
void MoveEnemyBullet(void);
void EnemyCrash(void);
void PlayerCrash(void);
void DrawInfo(void);
void Life_init(void);
void Life_Decreasing(void);
void next_stage(void);
void infoScreen(void);
void EraseEnemyBulletLv2(void);

void infoScreen(void)
{
	char edge1[20];
	char sdfsdf[40];
	char ddd[40];

	sprintf(level,"level: %d", Level);
	sprintf(score,"score: %2d ", Score);

	sprintf(edge1,"- - - - - - - - - -");
	sprintf(sdfsdf, "|    %s     |",level);
	sprintf(ddd, "|    %s   |",score);
	GLCD_SetTextColor(Olive);
	GLCD_DisplayString(15,12,0,edge1);
	GLCD_DisplayString(16,12,0,sdfsdf);
	GLCD_DisplayString(17,12,0,ddd);
	GLCD_DisplayString(18,12,0,edge1);
}

void next_stage(void)
{
	char buf[20];
	int i,j;	
	INT32U  cnts;
	GLCD_Clear(Black);
	DrawPlayer();
	for(i=py;i>0;){
		ErasePlayer();
		for(j=0 ; j<3000000 ; j++);
		i -= 3;
		py -= 3;
		DrawPlayer();
		for(j=0 ; j<3000000 ; j++);
	}	
	Level=2;
}

void pause(void* pdata)
{
	int flag=0;
	char push[40];
	static int tamperbut1=0; // not pressed
	INT8U  err;
	
	#if OS_CRITICAL_METHOD == 3
				OS_CPU_SR cpu_sr;
				#endif 

	sprintf(push, "PUSH TAMPER TO CONTINUE");
	
	while(1){

		if(flag==0){
	OS_ENTER_CRITICAL();
			OSSemPend(tamperbut_sem,0,&err);
			GLCD_Clear(Black);
			GLCD_SetTextColor(Olive);	
			GLCD_DisplayString(22,10,0,push);
			infoScreen();
			flag=1;

		} 

		if(tamperbut1==0&& !TAMPERBUT){
			tamperbut1=1;
		} else if(tamperbut1==1&& TAMPERBUT){
			tamperbut1=0;
			GLCD_Clear(Black);
			flag=0;
			
			OS_EXIT_CRITICAL();
			
			OSSemPost(done_sem);
		}
		
	
	}

}

void Life_Decreasing(void)
{
	if(life==3){
		GPIOG->BSRRH = 1<<  led--;
	}
	else if(life==2){
		GPIOG->BSRRH = 1<<  led--;
	}

	else{
		GPIOG->BSRRH = 1<<  led;	
	}
	life--; 
}

char getch(void)
{
	static int wakeupbut=0;// not pressed
	static int userbut=0;// not pressed
	static int tamperbut=0; // not pressed

	static int joy_left_state=INITIAL;
	static int joy_left_count=0;
	static int joy_right_state=INITIAL;
	static int joy_right_count=0;
	static int joy_up_state=INITIAL;
	static int joy_up_count=0;
	static int joy_down_state=INITIAL;
	static int joy_down_count=0;

	//총알
	if(WAKEUPBUT ){
		wakeupbut=1; //pressed
	} else if(wakeupbut==1 && !WAKEUPBUT){
		wakeupbut=0;
		return SPACE;
	}	

	//일시정지(세마포사용)	
	if (tamperbut==0 && !TAMPERBUT) {
		tamperbut=1; // pressed
	} else if (tamperbut==1 && TAMPERBUT) {
		INT8U  err;
		tamperbut=0; // not pressed
		OSSemPost(tamperbut_sem);// 게임 일시정지, 인포 테스크 실행
		OSSemPend(done_sem,0,&err);
		DrawInfo();

	}

	//게임 종료
	if(userbut==0 && !USERBUT) {
		userbut=1;
	}
	else if(userbut==1 && USERBUT) {
		userbut=0;
		return ESC;
	}

	//왼쪽 이동
	if (JOY_GetKeys() == JOY_LEFT) {
		if ((++joy_left_count) == TH) {
			joy_left_count = 0;
			return LEFT;
		}
		joy_left_state = BUT_DOWN;
	}
	else if (JOY_GetKeys() != JOY_LEFT) {
		if (joy_left_state==BUT_DOWN) {
			joy_left_state= BUT_UP;
			joy_left_count = 0;
			return LEFT;
		}
	}

	//오른쪽 이동
	if (JOY_GetKeys() == JOY_RIGHT) {
		if ((++joy_right_count) == TH) {
			joy_right_count = 0;
			return RIGHT;
		}
		joy_right_state = BUT_DOWN;
	}
	else if (JOY_GetKeys() != JOY_RIGHT) {
		if (joy_right_state==BUT_DOWN) {

			joy_right_count = 0;
			joy_right_state= BUT_UP;
			return RIGHT;
		}
	}

	//위로 이동
	if (JOY_GetKeys() == JOY_UP) {
		if ((++joy_up_count) == TH) {
			joy_up_count = 0;
			return UP;
		}
		joy_up_state = BUT_DOWN;
	}
	else if (JOY_GetKeys() != JOY_UP) {
		if (joy_up_state==BUT_DOWN) {

			joy_up_count = 0;
			joy_up_state= BUT_UP;
			return UP;
		}
	}

	//아래로 이동
	if (JOY_GetKeys() == JOY_DOWN) {
		if ((++joy_down_count) == TH) {
			joy_down_count = 0;
			return DOWN;
		}
		joy_down_state = BUT_DOWN;
	}
	else if (JOY_GetKeys() != JOY_DOWN) {
		if (joy_down_state==BUT_DOWN) {

			joy_down_count = 0;
			joy_down_state= BUT_UP;
			return DOWN;
		}
	}

	return 0;
}

void LoadingStage()
{
	static int tamperbut1=0;
	
	int i,j;
	char buf[40];
	
	for (i = 5; i < 35; i += 2) {
		GLCD_SetTextColor(White);
		GLCD_DisplayChar(4,i,0,'-');
		for(j=0 ; j<3000000 ; j++);
	}

	for (i = 5; i < 15; i += 2) {

		GLCD_SetTextColor(Green);
		GLCD_DisplayChar(i,34,0,'|');
		for(j=0 ; j<3000000 ; j++);
	}

	for (i = 33; i >= 4; i -= 2) {

		GLCD_SetTextColor(White);
		GLCD_DisplayChar(14,i,0,'-');
		for(j=0 ; j<3000000 ; j++);
	}

	for (i = 13; i > 4; i-=2) {

		GLCD_SetTextColor(Red);
		GLCD_DisplayChar(i,4,0,'|');
		for(j=0 ; j<3000000 ; j++);
	}

	GLCD_DisplayChar(9,7,0,'I');  for(j=0 ; j<3000000 ; j++);
	GLCD_DisplayChar(9,11,0,'N');  for(j=0 ; j<3000000 ; j++);
	GLCD_DisplayChar(9,15,0,'V');  for(j=0 ; j<3000000 ; j++);
	GLCD_DisplayChar(9,19,0,'A');  for(j=0 ; j<3000000 ; j++);
	GLCD_DisplayChar(9,23,0,'D');  for(j=0 ; j<3000000 ; j++);
	GLCD_DisplayChar(9,27,0,'E');  for(j=0 ; j<3000000 ; j++);
	GLCD_DisplayChar(9,31,0,'R');  for(j=0 ; j<3000000 ; j++);

	GLCD_SetTextColor(White);
	sprintf(buf, "GAME START");
	GLCD_DisplayString(23,15,0,buf);
	
	for(j=0;j<80000000;j++);
}


void HeightLine()
{
	int i;

	for (i = 0; i < 40; i++) {
		GLCD_SetTextColor(White);
		GLCD_DisplayChar(i,25,0,'|');
	}
	sprintf(level,"level: %d", Level);
	GLCD_DisplayString(14,29,0,level);
	sprintf(score,"score: %d", Score);
	GLCD_DisplayString(15, 29,0, score);
}


void DrawInfo(void)
{
	char title1[40], title2[40], title3[40];

	char joystick1[2],joystick2[10];
	char k1[20];
	char maker[40];
	GLCD_SetTextColor(White);

	sprintf(title1,"- - - - - - -");
	sprintf(title2,"INVADER");
	sprintf(title3,"- - - - - - -");


	sprintf(joystick1,"|");
	sprintf(joystick2,"-   -");

	sprintf(k1,"WAKEUP : *");
	GLCD_DisplayString(22, 29, 0, k1);
	sprintf(k1,"TAMPER : %c%c",'!','!');
	GLCD_DisplayString(23, 29, 0, k1);
	sprintf(k1,"USER : EXIT");
	GLCD_DisplayString(24, 29, 0, k1);

	sprintf(maker,"12 Hyunji HAM");
	GLCD_DisplayString(30, 26, 0, maker);
	sprintf(maker,"13 Sangeun LEE");
	GLCD_DisplayString(31, 26, 0, maker);

	GLCD_DisplayString(5, 26, 0, title1);
	GLCD_DisplayString(6, 29, 0, title2);
	GLCD_DisplayString(7, 26, 0, title3);


	GLCD_DisplayString(18, 32, 0, joystick1);
	GLCD_DisplayString(19,30,0,joystick2);
	GLCD_DisplayString(20, 32, 0, joystick1);

}

void ErasePlayer()
{
	char buf[40];
	sprintf(buf,"         ");
	GLCD_DisplayString(py,px-2,0,buf);

}
void DrawPlayer()
{
	char buf[40];
	GLCD_SetTextColor(Green);
	sprintf(buf,"<<>>");
	GLCD_DisplayString(py,px-2,0,buf);

}
void DrawPlayer_grace()
{
	char buf[40];
	GLCD_SetTextColor(White);
	sprintf(buf,"<<>>");
	GLCD_DisplayString(py,px-2,0,buf);

}

void PlayerMoveAction()
{
	int i, ch;
	DrawPlayer();

	ch = getch();
	switch (ch) {
	case SPACE:
		for (i = 0; i < MAXBULLET&&Bullet[i].exist == TRUE; i++) { ; }
		if (i != MAXBULLET) {
			Bullet[i].x = px;
			Bullet[i].y = py+1;//38;
			Bullet[i].exist = TRUE;
		}
		break;

	case LEFT:
		if(px==-1)
			break;
		ErasePlayer();
		px--;
		break;

	case RIGHT:
		if(px==22) break;
		ErasePlayer();
		px++;
		break;

	case UP:
		if(py==1) break;
		ErasePlayer();
		py--;

		break;

	case DOWN:
		if(py==39) break;
		ErasePlayer();
		py++;
		break;

	case ESC:
		exit(0);
		break;

	default:
		break;
	}
	if(is_collision==FALSE)
		DrawPlayer();
	else
		DrawPlayer_grace();
}


void DrawBullet(int i) 
{
	GLCD_SetTextColor(Green);
	GLCD_DisplayChar(Bullet[i].y, Bullet[i].x, 0, '*' );
}


void EraseBullet(int i) 
{
	GLCD_DisplayChar(Bullet[i].y, Bullet[i].x, 0, ' ' );
}

void PlayerBulletAction() 
{
	int i;
	for (i = 0; i < MAXBULLET; i++) {
		if (Bullet[i].exist == TRUE) {
			EraseBullet(i);
			if (Bullet[i].y == 1) {
				Bullet[i].exist = FALSE;
			}
			else {
				if(--Bullet[i].y>0)
					DrawBullet(i);
			}
		}
	}
}



//적군 만들기
void DrawEnemy(void) {
	int found;
	int i, j;
	char buf[20];

	for(i=0;i<MAXENEMY;i++){
		if(Enemy[i].exist==FALSE){
			Enemy[i].x=3+10*i;
			Enemy[i].y=1;
			Enemy[i].exist = TRUE;
			Enemy[i].type=1;
			Enemy[i].nBull=20; //총알이 발사되는 주기 설정
			Enemy[i].nFrame=Enemy[i].nStay=Enemy[i].move_count=50;
		}
		if(random(2)==0)
			Enemy[i].dest=1;
		else
			Enemy[i].dest=-1;
	}
}



//적군 총 발사
void EnemyBulletAction(int i) {
	int j;
	if (1) {//random(10) == 0
		for (j = i; j < MAXENEMYBULLET && EnemyBullet[j].exist == TRUE; j+=MAXENEMY);

		if (j != MAXENEMYBULLET) {
			EnemyBullet[j].x = Enemy[i].x-1;
			EnemyBullet[j].y = Enemy[i].y + 1 ;
			EnemyBullet[j].nFrame = EnemyBullet[j].nStay = 8;
			EnemyBullet[j].exist = TRUE;
		}
	}
}

//적군 이동 
void EnemyMoveAction(void) {
	int i;
	char buf[40];

	for (i = 0; i < MAXENEMY; i++)
	{
		if (Enemy[i].exist == FALSE)
			continue;

		if(Enemy[i].x>24||Enemy[i].x<0){ //적군이 화면밖을 벗어날 경우
			Enemy[i].exist=FALSE;
			sprintf(buf,"   ");
			GLCD_DisplayString( Enemy[i].y, Enemy[i].x,0,buf);
		}
		else{
			if (Enemy[i].type == -1) {  //일정기간 지나기 전에 총알 지나가서 +10 표시 없어지지 않도록
				GLCD_SetTextColor(Green);
				sprintf(buf,"+10");
				GLCD_DisplayString(Enemy[i].y, Enemy[i].x-2, 0, buf );
				if(--Enemy[i].nStay==0){  //일정 기간 후에 +10표시 지워줌
					Enemy[i].exist = FALSE;
					sprintf(buf,"   ");
					GLCD_DisplayString( Enemy[i].y, Enemy[i].x-2,0,buf);
					Enemy[i].nStay=Enemy[i].nFrame;
				}
				continue;
			}

			if(--Enemy[i].move_count==0){ //움직일 타이밍
				Enemy[i].move_count=Enemy[i].nFrame;
				sprintf(buf,"   ");
				GLCD_DisplayString( Enemy[i].y, Enemy[i].x-2,0,buf);
				Enemy[i].x+=Enemy[i].dest;
			}

			sprintf(buf,"%s",arrEnemy[Enemy[i].type]);
			GLCD_DisplayString( Enemy[i].y,Enemy[i].x-2,0,buf);
			if(--Enemy[i].nBull==0){
				Enemy[i].nBull=20; 		
				EnemyBulletAction(i);
			}
		}
	}
}


void EraseEnemyBullet(int i)
{
	GLCD_DisplayChar(EnemyBullet[i].y, EnemyBullet[i].x, 0, ' ' );
}

void DrawEnemyBullet(int i)
{

	GLCD_SetTextColor(Red);
	GLCD_DisplayChar(EnemyBullet[i].y, EnemyBullet[i].x, 0, '*' );
}

void DrawEnemyBullet_grace(int i)
{

	GLCD_SetTextColor(White);
	GLCD_DisplayChar(EnemyBullet[i].y, EnemyBullet[i].x, 0, '*' );
}

void MoveEnemyBullet(void) 
{
	int i;
	for ( i = 0; i < MAXENEMYBULLET; i++)
	{
		if (EnemyBullet[i].exist == FALSE)
			continue;

		if (--EnemyBullet[i].nStay == 0) {
			EnemyBullet[i].nStay = EnemyBullet[i].nFrame;
			EraseEnemyBullet(i);

			if (EnemyBullet[i].y > 38 )
				EnemyBullet[i].exist = FALSE;
			else {
				
				if(EnemyBullet[i].exist==-1)
					continue;
				EnemyBullet[i].y += 2;

				if(EnemyBullet[i].y-py>=1 && grace_period>=60 ){ //충돌 끝남
					is_collision=FALSE;
					grace_period=0;
				}
				if(is_collision==FALSE)
					DrawEnemyBullet(i);
				else
					DrawEnemyBullet_grace(i);
			}
		}
	}
}


void EnemyCrash() 
{
	int i,j;
	char buf[20];
	for ( i = 0; i < MAXENEMY; i++)
	{
		if (Enemy[i].exist == FALSE || Enemy[i].type == -1)
			continue;
		
		if (Bullet[i].exist==TRUE&&Enemy[i].y == Bullet[i].y && abs(Enemy[i].x - Bullet[i].x) <= 2) { 
			GLCD_DisplayChar(Bullet[i].y, Bullet[i].x, 0, ' ' );  //+10 뜨기 전에 적군 지워주는 용도
			Bullet[i].exist = FALSE;
			Enemy[i].type = -1;

			GLCD_SetTextColor(Green);
			sprintf(buf,"+10");
			GLCD_DisplayString(Enemy[i].y, Enemy[i].x-2, 0, buf );
			Score += 10;

			if(Score == 50){
				INT32U  cnts;
				Level=2;

				next_stage();
				infoScreen();
				px=10; py=39;
				for(j=0 ; j<50000000 ; j++);
				GLCD_Clear(Black);
				is_collision=TRUE;

					SystemCoreClockUpdate();
				  cnts = SystemCoreClock / 20;
					OS_CPU_SysTickInit(cnts);

				DrawInfo();
			}
			else if(Score ==80){
				#if OS_CRITICAL_METHOD == 3
			OS_CPU_SR cpu_sr;
			#endif 
				OS_ENTER_CRITICAL();
				next_stage();

				GLCD_Clear(Black);
				GLCD_SetTextColor(Olive);
				sprintf(buf,"YOU WIN!");
				GLCD_DisplayString(13,16,0,buf);
				infoScreen();
				for(i=0 ; i<1000000 ; i++);
				exit(0);
				OS_EXIT_CRITICAL();
			}
			break;
		}

	}
}

void PlayerCrash() 
{
	int i,j;
	char buf[20];
	
	for ( i = 0; i < MAXENEMYBULLET; i++)
	{
		if (EnemyBullet[i].exist == FALSE)
			continue;

		if (abs(EnemyBullet[i].y-py)<=1&&abs(EnemyBullet[i].x - px) <= 2) {
			is_collision=TRUE;

			GLCD_SetTextColor(Red);
			sprintf(buf,"BANG");
			GLCD_DisplayString(py, px - 2, 0, buf );	
	

			Life_Decreasing();
			if(life==0) GameOver=1;

			EnemyBullet[i].exist=FALSE;
			EraseEnemyBullet(i);
		}
		
	}
	
			
		if(Level==2){
			if (abs(EnemyBulletLv2.y-py)<=1&&abs(EnemyBulletLv2.x - px) <= 2) {
			is_collision=TRUE;

			GLCD_SetTextColor(Red);
			sprintf(buf,"BANG");
			GLCD_DisplayString(py, px - 2, 0, buf );		

			Life_Decreasing();
			if(life==0) GameOver=1;

			EnemyBulletLv2.exist=FALSE;
			EraseEnemyBulletLv2();

		}
			
		}
}



void Life_init(void){
	GPIOG->BSRRL = 1<<led++;
	GPIOG->BSRRL = 1<<led++;
	GPIOG->BSRRL = 1<<led;
}

void Start(void){
	GLCD_Clear(Black);
	LED_Init();
	Life_init();
	DrawInfo();
}
void game_main(void* pdata)
{
	int i;
	char buf[20];


	Start();


	while (!GameOver ) {

		HeightLine();


		PlayerMoveAction();
		PlayerBulletAction();
		DrawEnemy();
		EnemyMoveAction();

		MoveEnemyBullet();

		EnemyCrash();
		if(is_collision==0)  PlayerCrash();
		else grace_period++;

		for(i=0 ; i<1000000 ; i++);
	}


	GLCD_Clear(Black);
	for(i=0 ; i<1000000 ; i++);
	GLCD_SetTextColor(Olive);
	sprintf(buf,"GAME OVER");
	GLCD_DisplayString(13,16,0,buf);
	infoScreen();
	for(i=0 ; i<1000000 ; i++);
	exit(0);
}
