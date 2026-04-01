#include "util.h"

const char* dst_ip = "127.0.0.1"; // 하나의 host안에서 통신할 것이므로 서버주소는 localhost(i.e., 127.0.0.1)임
#define NUM_SRV 2 //서버 개수

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


  struct KVS RecvMsg={0,}; // 수신용으로 쓸 메시지 구조체 생성 및 초기화 //새로 선언해도 되나?

	while(cnt < 5){
    printf("Request ID: %d\n",cnt++); //리퀘스트 아이디 출력

    generate_key(SendMsg.key); // key를 새로 생성한다.

    int partition = hash64(SendMsg.key) % NUM_SRV; //생성한 키에 hash64() 함수 적용, 서버 개수로 mod연산함 -> 파티션 번호 얻음
    //mod 연산 후 결과값이 작기 때문에 partition 자료형은 int로 선언해도 무방하다.

    //얻은 파티션 번호에 포트번호를 더함. -> 어떤 서버로 보낼지 결정
    int serverNum = partition + SERVER_PORT;

    srv_addr.sin_port = htons(serverNum); // 파티션에 따라 포트 재설정 (이전에 설정했던 값을 덮어씀)

    //파티션 번호 출력하는 함수
    printf("Partition: %d\n", partition);

    //보낼 때 요청이 읽기 요청인지 쓰기 요청인지는 랜덤하게 결정됨
    int req = ( rand() % 2 ) * 2; //나머지 0, 1에 2곱하면 0, 2됨
    // #define READ_REQ 0, #define WRITE_REQ 2 와 대응될 수 있다.


    if (req == READ_REQ) //읽기 요청시 (서버에 저장돼있는 value 필드를 읽어와야 하기 때문에 비워서 보냄)
    {
      SendMsg.type = READ_REQ; // 요청 타입을 읽기로 선언한다.
      strncpy(SendMsg.value, "", VALUE_SIZE-1); // value필드 비워줌
      SendMsg.value[VALUE_SIZE - 1] = '\0';  // 명시적으로 널 터미네이터 추가
      printf("Sent bytes: %ld\n", sizeof(uint8_t) + KEY_SIZE);
      //구조체 크기로 계산하면 value 필드가 비워져 있어도 기본으로 할당되어있는 메모리 크기가 계산되므로 항상 33byte가 나옴. 따로 분리해서 계산후 출력해야함

    }

    else if (req == WRITE_REQ) //쓰기 요청시 (key-value 저장소에 저장해야하는 value 필드 전송)
    {
      SendMsg.type = WRITE_REQ; //요청 타입을 쓰기로 선언한다.
      strncpy(SendMsg.value, "AAAABBBBCCCCDDD", VALUE_SIZE-1); // value필드에 미리 생성해둔 value 값을 복사한다.
      SendMsg.value[VALUE_SIZE - 1] = '\0';  // 명시적으로 널 터미네이터 추가
      printf("Sent bytes: %ld\n", sizeof(SendMsg)); //보낸 메시지의 크기 계산해서 출력

    }


    sendto(sock, &SendMsg, sizeof(SendMsg), 0, (struct sockaddr *)&srv_addr, sizeof(srv_addr)); // 생성한 메시지를 서버로 송신한다.
    printf("Type: %s Key: %s Value: %s\n", get_type(SendMsg), SendMsg.key, SendMsg.value); //(보낼 때)생성한 key와 value를 출력해본다.

    //서버로부터 답장을 수신한다.
    n = recvfrom(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&src_addr, &src_addr_len); // 서버로부터 답장을 수신한다.

    //수신한 바이트 수 출력
    if (RecvMsg.type == READ_REP){ //읽어온 경우 value를 채워왔을테니 33바이트 출력
            printf("Received bytes: %ld\n", sizeof(SendMsg)); //응답받은 메시지의 크기 계산해서 출력
            } else if(RecvMsg.type == WRITE_REP){ //쓰고온 경우 value를 비우고 왔을테니 17바이트 출력
              printf("Received bytes: %ld\n", sizeof(uint8_t) + KEY_SIZE);//응답받은 메시지의 크기 계산해서 출력
             }

    printf("Type: %s Key: %s Value: %s\n",get_type(RecvMsg),RecvMsg.key, RecvMsg.value); // 수신한 내용을 출력한다.
	
            }

	close(sock); // 소켓을 닫아준다.
	return 0;
}
