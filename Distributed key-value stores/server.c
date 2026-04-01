#include "util.h"

int SERVER_PORT; // 서버 포트번호
char* kv[DATASET_SIZE]; // 정적 key value stores
void init_kvs(){ //기본 value값을 가지고 있도록 해줌
  for(int i =0;i<DATASET_SIZE;i++){
		kv[i] = malloc(VALUE_SIZE);
		strcpy(kv[i], "DDDCCCCBBBBAAAA");
		//printf("%s\n",kv[i]);
	}

} 

static volatile int quit = 0; // Trigger conditions for SIGINT
void signal_handler(int signum) {
	if(signum == SIGINT){  // Functions for Ctrl+C (SIGINT)
		quit = 1;
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

	init_kvs(); // key-value store 초기화 -> 기본값으로 채워 넣음

	int n = 0;
 	struct KVS RecvMsg; // 수신용으로 쓸 메시지 구조체 생성

	struct sockaddr_in src_addr; // 패킷을 수신하였을 때, 해당 패킷을 보낸 송신자(Source)의 정보를 저장하기 위한 소켓 구조체
  	socklen_t src_addr_len = sizeof(src_addr);
	size_t pkt_size = 0;

	while (!quit) {
	//서버측에서 클라이언트의 요청 받음
	//클라이언트에서 SendMsg 구조체로 전송한 데이터를 서버에서 RecvMsg 구조체에 복사해 받음
	n = recvfrom(sock, &RecvMsg, sizeof(RecvMsg), 0,(struct sockaddr *)&src_addr, &src_addr_len);

	// hash()로 키 인덱스 계산
	// DATASET_SIZE로 % 연산해서 인덱스 얻음
	uint64_t index = hash64(RecvMsg.key) % DATASET_SIZE;// 서버의 메모리 안에서 어디에 데이터를 저장하거나 읽을지 위치를 계산해야되므로

	// 응답 보내는 코드 작성해야함(받은 요청이 읽기/쓰기인 경우 분기)
    if (RecvMsg.type == READ_REQ) { //받은 요청이 읽기 요청일 경우

	  printf("Type: %s Key: %s Value: %s\n",get_type(RecvMsg),RecvMsg.key, RecvMsg.value); // 수신한 내용을 출력한다.

	  // 응답할 때 value 필드 채워서 보내야함
	  strncpy(RecvMsg.value, kv[index], VALUE_SIZE); //value 필드에 초기화된 기본값 채움

	  RecvMsg.type = READ_REP; // 보내는 응답 타입을 읽기 응답으로 선언한다.
	  //클라이언트에게 응답 전송
	  sendto(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&src_addr, src_addr_len);
	  // src_addr_len: 주소 크기 값 자체만 주면 됨, sizeof(srv_addr)보다 recvfrom에서 받은 실제 주소 크기인 src_addr_len을 사용
    }

    else if (RecvMsg.type == WRITE_REQ){ //받은 요청이 쓰기 요청일 경우

	  printf("Type: %s Key: %s Value: %s\n",get_type(RecvMsg),RecvMsg.key, RecvMsg.value); // 수신한 내용을 출력한다.


	  strncpy(kv[index], RecvMsg.value, VALUE_SIZE); //쓰기요청이므로 클라이언트 측에서 보낸 value 값을 kv에 저장한다
	  strncpy(RecvMsg.value, "", VALUE_SIZE); //응답 보낼 땐 value 필드를 비워서 보내야 한다.
      
	  RecvMsg.type = WRITE_REP; //보내는 응답 타입을 쓰기 응답으로 선언한다.
	  sendto(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&src_addr, src_addr_len);
	  // src_addr_len: 주소 크기 값 자체만 주면 됨, sizeof(srv_addr)보다 recvfrom에서 받은 실제 주소 크기인 src_addr_len을 사용
    }

	}

	printf("\nCtrl+C pressed. Exit the program after closing the socket\n");
	close(sock);

	return 0;
}
