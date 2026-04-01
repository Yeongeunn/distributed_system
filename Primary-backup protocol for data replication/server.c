#include "util.h"

int SERVER_PORT; // 서버 포트번호
char* kv[DATASET_SIZE]; // 정적 key value stores
void init_kvs(){
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

	// 서버의 정보를 담을 소켓 구조체 생성 및 초기화 -> 이건 리더쪽 내 소켓
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

	init_kvs(); // key-value store 초기화

	int n = 0;
  	struct KVS RecvMsg={0,}; // 수신용으로 쓸 메시지 구조체 생성 및 초기화

	struct sockaddr_in src_addr; // 패킷을 수신하였을 때, 해당 패킷을 보낸 송신자(Source)의 정보를 저장하기 위한 소켓 구조체
  	socklen_t src_addr_len = sizeof(src_addr);
	size_t pkt_size = 0;

	// 팔로워로부터 ACK 2개 받기
	int ack_count = 0; //총 2개의 ACK를 받아야 하니까, 카운터 시작
	struct KVS AckMsg = {0,}; //ack 수신용 구조체 초기화
	struct sockaddr_in ack_addr;
	socklen_t ack_len = sizeof(ack_addr);

	while (!quit) {
		//리더서버 포트번호 5001 사용중
		//팔로워 서버 포트번호 5002, 5003 사용중

		//클라이언트로부터 패킷 수신함
		n = recvfrom(sock, &RecvMsg, sizeof(RecvMsg), MSG_DONTWAIT, (struct sockaddr *)&src_addr, &src_addr_len);
		if (n <= 0) continue; //수신한 바이트 수가 0보다 클 때만 처리함. 0보다 작으면 밑의 코드로 넘어가지 않음


		uint64_t index = hash64(RecvMsg.key) % DATASET_SIZE; // 인덱스는 key에 해시함수를 적용하고 DASET_SIZE로 modulo 연산을 수행해서 구한다.

		// FOLLOWER 서버 동작
		if (SERVER_PORT == 5002 || SERVER_PORT == 5003) { //만약 커맨드에서 5001, 5002번 포트로 입력이 들어왔다면
			if (RecvMsg.type == WRITE_REQ) { //리더 서버로부터 받은 패킷 메시지 타입이 읽기요청일 경우
				//받은 패킷의 내용을 출력한다
				printf("Type: %s Key: %s Value: %s\n", get_type(RecvMsg), RecvMsg.key, RecvMsg.value);
				//요청에서 받은 패킷의 value 필드에 있는 값을 팔로워 서버의 저장소에 복사한다.
				strncpy(kv[index], RecvMsg.value, VALUE_SIZE);

				RecvMsg.type = WRITE_REP; //패킷 타입을 쓰기응답으로 바꾼다
				strncpy(RecvMsg.value, "", VALUE_SIZE); //저장소에 썼기 때문에 value값을 비워준다.

				//recvfrom()으로 받은 src_addr가 이미 리더 서버(5001)의 주소임 명시적으로 5001번으로 설정해서 보낼 필요 없음
				sendto(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&src_addr, src_addr_len); //리더 서버에게 ACK 보낸다

				printf("Write is Done\n"); // FOLLOWER 완료 메시지
			}
			continue;// FOLLOWER는 WRITE_REQ만 처리하므로 루프 다시 시작한다.
			// READ_REQ를 받을 일도 없고, 혹시라도 누가 READ_REQ를 보내도 무시해야 함
		}


		//리더 서버가 받은 요청 처리
		if (RecvMsg.type == READ_REQ){ //만약 받은 요청이 읽기 요청이라면

			// 받은 패킷 내용 출력
			printf("Type: %s Key: %s Value: %s\n",get_type(RecvMsg),RecvMsg.key, RecvMsg.value); // 수신한 내용을 출력한다.

			RecvMsg.type = READ_REP; //받은 패킷의 타입을 읽기 응답으로 변경

			strcpy(RecvMsg.value, kv[index]); // 키에 해당하는 값을 kv에서 찾아 복사
			
			pkt_size = n + VALUE_SIZE; //서버의 데이터를 담아서 보내므로 응답 보낼 패킷의 사이즈에 value 사이즈 추가한다
			
			//클라이언트에게 읽기 응답을 보낸다
			sendto(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&src_addr, src_addr_len);

		}

		else if (RecvMsg.type == WRITE_REQ){ //받은 요청이 쓰기 요청일 경우

		//클라이언트에게서 받은 요청 내용을 출력한다
		printf("Type: %s Key: %s Value: %s\n",get_type(RecvMsg),RecvMsg.key, RecvMsg.value); // 수신한 내용을 출력한다.
		
		strncpy(kv[index], RecvMsg.value, VALUE_SIZE); //쓰기요청이므로 클라이언트 측에서 보낸 value 값을 kv에 저장한다


		// 팔로워 2명에게 똑같은 쓰기 요청 전송
		struct sockaddr_in follower_addr[2]; //2개의 팔로워 서버에게 보내야함. 배열 선언
		int follower_ports[2] = {5002, 5003}; //5001번 포트, 5002번 포트를 배열에 저장함

		// 각 팔로워 서버의 주소(IP + 포트)를 설정한다.
		// 리더 서버가 follower에게 sendto()로 쓰기 요청을 보내기 위해선 대상 서버의 IP 주소와 포트 번호가 정확히 설정되어 있어야 함.
		for (int i = 0; i < 2; i++) {
			memset(&follower_addr[i], 0, sizeof(follower_addr[i])); // 주소 초기화
			follower_addr[i].sin_family = AF_INET; // IPv4 사용
			follower_addr[i].sin_port = htons(follower_ports[i]); // 포트 번호 (5002, 5003)
			follower_addr[i].sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

			//설정한 주소로 팔로워에게 쓰기 요청 전송
			sendto(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&follower_addr[i], sizeof(follower_addr[i]));
		}

		while (ack_count < 2) {

			//follower들이 보내는 응답(ACK)을 수신 시도
			int r = recvfrom(sock, &AckMsg, sizeof(AckMsg), MSG_DONTWAIT, (struct sockaddr *)&ack_addr, &ack_len);
			//MSG_DONTWAIT -> 블로킹하지 않고 non-blocking 수신 (없으면 넘어감)

			//패킷이 실제로 도착한 경우만 처리, follower가 보내온 응답이 WRITE_REP인지 확인
			if (r > 0 && AckMsg.type == WRITE_REP) {
				ack_count++; //응답이 맞으면 카운트 증가
			}
		}

		printf("Write is Done\n"); // 리더 서버도 완료 메시지 출력


		// 리더 서버(포트 5001)가 클라이언트에게 응답 전송 (value는 비운다)

		RecvMsg.type = WRITE_REP; //보내는 응답 타입을 쓰기 응답으로 선언한다.

		strncpy(RecvMsg.value, "", VALUE_SIZE); //응답 보낼 땐 value 필드를 비워서 보내야 한다.
					
		pkt_size = sizeof(struct KVS) - VALUE_SIZE; // value 필드를 비웠으므로 명시적으로 value 사이즈를 줄인다. 그냥 구조체 보내면 pedding때문에 값이 없어도 value 필드의 사이즈까지 포함돼서 출력되기 때문
		
		sendto(sock, &RecvMsg, pkt_size, 0, (struct sockaddr *)&src_addr, src_addr_len);
	
		}
			
	
			strcpy(RecvMsg.value,""); // 버퍼 초기화(다음 요청을 위해)
		}

	printf("\nCtrl+C pressed. Exit the program after closing the socket\n");
	close(sock);

	return 0;
}



		//리더 서버가 쓰기요청을 받았을 때
		//리더서버가 클라이언트에게서 받은 내용을 그대로 출력하고
		//리더 서버의 키밸류 스토어에 받은 값을 저장함
		//팔로워 서버 두 개에 클라이언트에서 받은걸 그대로 또 쓰기요청을 보내면 됨(리더가 팔로워 2개에게 보냄)
		//팔로워 두 개가 쓰기요청 받음
		//각각의 팔로워 서버에서 쓰기요청받은거 출력함
		//각 팔로워서버는 쓰기요청 받은 값을 자신의 키밸류 스토어에 업데이트
		//각 팔로워 서버는 리더서버에게 다 썼다고 ack 보냄
		//글고 각 팔로워 서버도 Write is Done출력도 함
		//리더서버는 두 서버 모두에게서 다 썻다는 ack을 받으면
		//리더서버도 Write is Done 출력함
		//클라이언트에게 읽기 응답을 보내기 전, value값 비움, 사이즈도 value필드 줄여서 보냄
		//그리고 클라이언트에게 읽기 응답을 보냄

		//리더 서버가 읽기 요청을 받았을 때
		//리더서버가 클라이언트에게서 받은 내용을 그대로 출력하고
		//리더 서버에 저장돼있는 key-value값을 읽어서
		//패킷에 클라이언트에게 담아 보냄