#include "util.h"


int SERVER_PORT; // 서버 포트번호 

static volatile int quit = 0; // Trigger conditions for SIGINT
void signal_handler(int signum) {
	if(signum == SIGINT){  // Functions for Ctrl+C (SIGINT)
		quit = 1;
	}
} 

int main(int argc, char *argv[]) {


	srand((unsigned int)time(NULL));  // 난수 발생기 초기화

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
    struct KVS SendMsg={0,}; // 응답용으로 쓸 메시지 구조체 생성 및 초기화


	struct sockaddr_in src_addr; // 패킷을 수신하였을 때, 해당 패킷을 보낸 송신자(Source)의 정보를 저장하기 위한 소켓 구조체
  socklen_t src_addr_len = sizeof(src_addr);
	size_t pkt_size = 0;


	while (!quit) {

	//클라이언트의 요청 받음
	//클라이언트에서 SendMsg 구조체로 전송한 데이터를 서버에서 RecvMsg 구조체에 복사해 받음
	n = recvfrom(sock, &RecvMsg, sizeof(RecvMsg), 0,(struct sockaddr *)&src_addr, &src_addr_len);

	int hit = rand() % 2; // 0 또는 1 랜덤 값 -> 50% 확률


	  // 응답 보내는 코드

	  if(hit == 1) //CACHE_HIT일 경우
	  {
		RecvMsg.type = CACHE_HIT; // 보내는 응답 타입을 캐시 hit으로 선언한다.

		strncpy(RecvMsg.value, "CACHECACHECACHE", VALUE_SIZE - 1); //응답 보낼 때 value값 채워서 보낸다
		RecvMsg.value[VALUE_SIZE - 1] = '\0';// 명시적으로 널 터미네이터 추가

		printf("Cache Hit for Key: %s\n",RecvMsg.key); // 캐시 hit 여부와 수신한 키를 출력한다.

	  } else { //CACHE_MISS일 경우

		RecvMsg.type = CACHE_MISS; // 보내는 응답 타입을 캐시 hit으로 선언한다.

		strncpy(RecvMsg.value, "", VALUE_SIZE); //캐시 노드에서 담아갈 값은 없다.

		printf("Cache Miss for Key: %s\n",RecvMsg.key); // 캐시 hit 여부와 수신한 키를 출력한다.
	  

	  }

	  
	  	//클라이언트에게 응답 전송. 캐시 힛: CACHECACHECACHE 캐시 미스: value 빈 값
		sendto(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&src_addr, src_addr_len);
		// src_addr_len: 주소 크기 값 자체만 주면 됨, sizeof(srv_addr)보다 recvfrom에서 받은 실제 주소 크기인 src_addr_len을 사용
		

	}

	printf("\nCtrl+C pressed. Exit the program after closing the socket\n");
	close(sock);

	return 0;
}
