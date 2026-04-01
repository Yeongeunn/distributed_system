#include "util.h"

const char* dst_ip = "127.0.0.1";
#define NUM_SRV 4

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
	int SERVER_PORT = 5001;
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

  //어느 서버에 몇 번 보냈는지 횟수 저장하는 배열 선언/초기화
  int ReqCnt[4] = {0, 0, 0, 0};  // index 0: 5001, 1: 5002, ~
	
  while(cnt < 5){

    if(cnt < 5) { // 패킷 5개를 전송한다.

      printf("Request ID: %d\n",cnt++); //리퀘스트 아이디 출력
      SendMsg.type = READ_REQ; //보내는 패킷의 요청을 읽기 요청으로 선언
      //srv_addr.sin_port = htons(serverNum); // 파티션에 따라 포트 재설정 (이전에 설정했던 값을 덮어씀)
      strcpy(SendMsg.value,""); //읽기 요청보낼 때 value 필드 비워서 보내야 함
      pkt_size = sizeof(struct KVS) - VALUE_SIZE; //패킷 사이즈에서 value 크기 뺀다
      
      generate_key(SendMsg.key); //랜덤 키 생성



      //서버의 개수와 각 서버의 포트 번호를 알고있음(4개임) -> #define NUM_SRV 4 포트번호는 5001~5004
      //누적요청수 기준 -> 클라이언트가 판단해야함 엥?
      
      // 두 서버를 무작위로 2개 뽑는다 (0~3)
      int sNum1 = rand() % NUM_SRV; // 서버 번호 즉, ReqCnt 인덱스 번호를 랜덤으로 구한다
      int sNum2 = rand() % NUM_SRV; // 서버 번호 즉, ReqCnt 인덱스 번호를 랜덤으로 구한다

      while (sNum1 == sNum2) //만약 랜덤으로 뽑은 서버의 번호가 같다면 아래 내용 계속 반복
              { sNum2 = rand() % NUM_SRV;} //두 번째의 서버 번호를 다시 랜덤으로 구한다.

      // 클라이언트가 해당 서버에 지금까지 몇 번 요청을 보냈는지를 비교 -> 더 적은 서버 선택

      int chosen; // 최종 선택된 서버

      if (ReqCnt[sNum1] < ReqCnt[sNum2]) { //만약 두 번째 선택된 서버의 요청 횟수가 더 많다면
          chosen = sNum1; //첫 번째로 선택된 서버에 요청을 보내야함
      }
      else if (ReqCnt[sNum1] > ReqCnt[sNum2]) { //만약 첫 번째 선택된 서버의 요청 횟수가 더 많다면
          chosen = sNum2; //두 번째로 선택된 서버에 요청을 보내야함
      }
      else {
          // 요청 수가 같으면 그냥 sNum1로 설정해서 보내기로
          chosen = sNum1;
      }
      
      ReqCnt[chosen]++;  // 선택된 서버의 요청 횟수 증가


      srv_addr.sin_port = htons(SERVER_PORT + chosen); // 목적지 서버 포트 설정


      // int partition = hash64(SendMsg.key) % NUM_SRV; //생성한 키에 hash64() 함수 적용, 서버 개수로 mod연산함 -> 파티션 번호 얻음
      
      // //헐잠만!! 이거 해야되는건가? 서버에서 입력값으로 서버 번호를 받을텐데
      // int serverNum = partition + SERVER_PORT; //얻은 파티션 번호에 포트번호를 더함. -> 어떤 서버로 보낼지 결정
      
      //전송 직접 정보 출력
      printf("Sent bytes: %ld\n",pkt_size); //보내는 패킷사이즈 출력
      printf("Type: %s Key: %s Value: %s\n",get_type(SendMsg),SendMsg.key,SendMsg.value); //보내는 패킷 내용 츨력
      

      //서버에 보냄
      //보낼 때 데이터 값에서 밸류 필드 크기 빼고 보내야함
    }


    //더 적은 요청을 받은 서버로 요청을 보냄
    sendto(sock, &SendMsg, pkt_size, 0, (struct sockaddr *)&srv_addr, sizeof(srv_addr)); // 생성한 메시지를 서버로 송신한다.



    struct KVS RecvMsg={0,}; // 수신용으로 쓸 메시지 구조체 생성 및 초기화
    n = recvfrom(sock, &RecvMsg, sizeof(RecvMsg), 0, (struct sockaddr *)&src_addr, &src_addr_len); // 서버로부터 답장을 수신한다.


		if (n > 0) { // 만약 송신한 데이터가 0바이트를 초과한다면 (즉, 1바이트라도 수신했다면)
      printf("Received bytes: %d\n",n);
      printf("Type: %s Key: %s Value: %s\n",get_type(RecvMsg),RecvMsg.key, RecvMsg.value); // 수신한 내용을 출력한다.
		}
    
	}

	close(sock); // 소켓을 닫아준다.
	return 0;
}
