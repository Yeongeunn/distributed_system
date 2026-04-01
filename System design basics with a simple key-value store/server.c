#include "util.h"

int SERVER_PORT; // 서버 포트번호

static volatile int quit = 0; // Trigger conditions for SIGINT
void signal_handler(int signum) {
	if(signum == SIGINT){  // Functions for Ctrl+C (SIGINT)
		quit = 1;
	}
}

//문자열 뒤집는 함수
void reverse(char* str, int len)
{
	int i = 0; //문자열 앞쪽 인덱스
	int j = len - 1; //문자열 뒤쪽 인덱스
	char temp; //임시 저장할 변수

	while (i<j){
		temp = str[i]; //앞쪽 문자 임시저장
		str[i] = str[j]; //뒤쪽 문자를 앞쪽 문자에 덮어쓰기
		str[j] = temp; //임시저장 변수에 있던 앞쪽 문자를 뒤쪽에 저장

		i++; //앞쪽 인덱스 오른쪽으로 이동
		j--; //뒤쪽 인덱스 왼쪽으로 이동
	}
}

int main(int argc, char *argv[]) {
	// 프로그램 시작시 입력받은 매개변수를 parsing한다. 
	if ( argc < 2 ){
	 printf("Input : %s port number\n", argv[0]);
	 return 1;
	}

	signal(SIGINT, signal_handler); // SIGINT에 대한 핸들러 등록

	SERVER_PORT = atoi(argv[1]); // 입력받은 argument를 포트번호 변수에 넣어준다.

	// 서버의 정보를 담을 소켓 구조체 생성 및 초기화
	struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(SERVER_PORT);
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0 i.e., 자기 자신의 IP

	// 소켓을 생성한다.
	int sock;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Could not create listen socket\n");
		exit(1);
	}

	// 생성한 소켓에 소켓구조체를 bind시킨다.
	if ((bind(sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) < 0) {
		printf("Could not bind socket\n");
		exit(1);
	}

	int n = 0;
  struct KVS RecvMsg={0,}; // 수신용으로 쓸 메시지 구조체 생성 및 초기화

	struct sockaddr_in src_addr; // 패킷을 수신하였을 때, 해당 패킷을 보낸 송신자(Source)의 정보를 저장하기 위한 소켓 구조체
  socklen_t src_addr_len = sizeof(src_addr);

	while (!quit) {

		//서버측에서 클라이언트의 요청 받음
		//클라이언트에서 SendMsg 구조체로 전송한 데이터를 서버에서 RecvMsg 구조체에 복사해 받음
		n = recvfrom(sock, &RecvMsg, sizeof(RecvMsg), 0,(struct sockaddr *)&src_addr, &src_addr_len);


		RecvMsg.type = READ_REP; //클라이언트가 보낸 요청을 응답으로 변경
		// SendMsg는 클라이언트쪽 구조체임 서버는 RecvMsg를 통해 요청을 받았기 때문에 RecvMsg를 수정하는 것
		
		//값 필드를 "DDDCCCCBBBBAAAA"로 변경할 것 -> 만든 함수 호출
		int value_len = strlen(RecvMsg.value); //받은 문자열 길이를 strlen을 이용해 계산
		reverse(RecvMsg.value, value_len); //받은 값 거꾸로 뒤집기

		//클라이언트에게 응답 전송. key는 그대로 보내고 value는 뒤집어서 보냄
		sendto(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&src_addr, src_addr_len);
		// src_addr_len: 주소 크기 값 자체만 주면 됨, sizeof(srv_addr)보다 recvfrom에서 받은 실제 주소 크기인 src_addr_len을 사용
	}

	printf("\nCtrl+C pressed. Exit the program after closing the socket\n");
	close(sock);

	return 0;
}
