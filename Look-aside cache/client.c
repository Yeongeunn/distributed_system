#include "util.h"

const char* dst_ip = "127.0.0.1"; // 하나의 host안에서 통신할 것이므로 서버주소는 localhost(i.e., 127.0.0.1)임

// 임의의 key를 생성해서 반환해줌
void generate_key(char* key) {
    uint64_t number = rand() % DATASET_SIZE;
    for (int i = 0; i < 5; ++i) number = ((number << 3) - number + 7) & 0xFFFFFFFFFFFFFFFF;
    key[KEY_SIZE - 1] = '\0';
    for (int i = KEY_SIZE - 2; i >= 0; i--) {
        int index = number % SET_SIZE;
        key[i] = SET[index];
        number /= SET_SIZE;
    }
} 

int main(int argc, char *argv[]) {

  srand((unsigned int)time(NULL));  // 난수 발생기 초기화
  /* 서버 구조체 설정 */
	int SERVER_PORT = 5001; // 입력받은 argument를 포트번호 변수에 넣어준다. 
	struct sockaddr_in srv_addr; // 패킷을 수신할 서버의 정보를 담을 소켓 구조체를 생성한다.
	memset(&srv_addr, 0, sizeof(srv_addr)); // 구조체를 모두 '0'으로 초기화해준다.
	srv_addr.sin_family = AF_INET; // IPv4를 사용할 것이므로 AF_INET으로 family를 지정한다.
	srv_addr.sin_port = htons(SERVER_PORT); // 서버의 포트번호를 넣어준다. 이 때 htons()를 통해 byte order를 network order로 변환한다.
	inet_pton(AF_INET, dst_ip, &srv_addr.sin_addr);  // 문자열인 IP주소를 바이너리로 변환한 후 소켓 구조체에 저장해준다.

  /* 소켓 생성 */
	int sock; // 소켓 디스크립터(socket descriptor)를 생성한다.
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { // socket()으로 IPv4(AF_INET), UDP(SOC_DGRAM)를 사용하는 소켓을 생성 시도한다.
		printf("Could not create socket\n"); // sock으로 return되는 값이 -1이라면 소켓 생성에 실패한 것이다.
		exit(1);
	}

	int n = 0;

  struct KVS SendMsg={0,}; // 송신용으로 쓸 메시지 구조체 생성 및 초기화

  struct sockaddr_in src_addr; // 패킷을 수신하였을 때, 해당 패킷을 보낸 송신자(Source)의 정보를 저장하기 위한 소켓 구조체
  socklen_t src_addr_len = sizeof(src_addr); // 수신한 패킷의 소켓 구조체 크기를 저장함. IPv4를 사용하므로 sockaddr_in 크기인 16바이트가 저장됨.
  int cnt = 0; // 패킷 5개를 전송한다.
  size_t pkt_size = 0;

  struct KVS RecvMsg={0,}; // 수신용으로 쓸 메시지 구조체 생성 및 초기화

	while(cnt < 5){
    printf("Request ID: %d\n",cnt++); //리퀘스트 아이디 출력

    generate_key(SendMsg.key); // key를 새로 생성한다.

    //읽기 요청만 있다! 읽기 요청으로 메시지타입 설정
    SendMsg.type = READ_REQ; 

    strncpy(SendMsg.value, "", VALUE_SIZE-1); // 읽기요청 보낼 때 value필드 비워줌
    SendMsg.value[VALUE_SIZE - 1] = '\0';  // 명시적으로 널 터미네이터 추가
    printf("Sent bytes: %ld\n", sizeof(uint8_t) + KEY_SIZE); //읽기요청이므로 value 필드 비어있음 -> 17바이트만 출력

    //보낼때 캐시 노드에 먼저 읽기요청 해야함. 위에서 5001 설정되어 있어도 한 번 cache miss였으면 포트번호 5002가 됨. 다시 5001로 초기화   
    srv_addr.sin_port = htons(5001);    //목적지인 캐시 노드로 요청 보낸다.
    sendto(sock, &SendMsg, sizeof(SendMsg), 0, (struct sockaddr *)&srv_addr, sizeof(srv_addr)); // 생성한 메시지를 서버로 송신한다.
    //캐시노드로 요청 보낸 내용 출력
    printf("Type: %s Key: %s Value: %s\n", get_type(SendMsg), SendMsg.key, SendMsg.value); //(보낼 때)생성한 key와 value를 출력해본다.




    //캐시노드로부터 답장을 수신한다.
    n = recvfrom(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&src_addr, &src_addr_len); // 서버로부터 답장을 수신한다.


    //캐시 노드의 응답 처리
    if(RecvMsg.type == CACHE_HIT)
    {
      //캐시노드의 응답 메시지 타입이 CACHE_HIT일 경우 해당 문자열 출력
      printf("Cache hit!!\n");

    } else if(RecvMsg.type == CACHE_MISS)
    {
      //캐시노드의 응답 메시지 타입이 CACHE_MISS일 경우 해당 문자열 출력
      printf("Cache Miss!\n");

      //cache miss시 저장소 서버(5002)에 동일한 요청을 보냄

      // 서버 주소로 포트 바꿔치기
      srv_addr.sin_port = htons(5002);

      //서버 5002에게 다시 요청보냄
      sendto(sock, &SendMsg, sizeof(SendMsg), 0, (struct sockaddr *)&srv_addr, sizeof(srv_addr)); // 생성한 메시지를 서버로 송신한다.

      //서버에서 보낸 응답 수신
      recvfrom(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&src_addr, &src_addr_len);


    }




    //수신한 바이트 수 출력. 캐시노드에서 캐시 hit으로 응답 받았으면 해당 값 채워오는 거고, 캐시 미스 후 서버에서 응답 받아왔으면 저장돼있는 값 채워옴.
    //클라이언트가 수신한 메시지는 항상 value값이 채워져있을 것이므로 33byte 출력해야함.
    printf("Received bytes: %ld\n", sizeof(SendMsg)); //응답받은 메시지의 크기 계산해서 출력. 33byte 출력


    //서버 or 캐시노드에서 보낸 응답 내용 출력
    printf("Type: %s Key: %s Value: %s\n\n",get_type(RecvMsg),RecvMsg.key, RecvMsg.value); // 수신한 내용을 출력한다.
	

	}

	close(sock); // 소켓을 닫아준다.
	return 0;
}
