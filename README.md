# OS HW_1 22100484 이건우
# 파일 기능 출력

<img width="1076" alt="스크린샷 2025-05-04 오후 2 57 01" src="https://github.com/user-attachments/assets/27bb8fd9-fbaf-4794-b9e7-874b7c50dcfb" />
<img width="1019" alt="스크린샷 2025-05-04 오후 3 00 59" src="https://github.com/user-attachments/assets/27c29fed-70c7-4d7d-bf5c-d9d190f379bb" />

# 목적 및 서비스 기능
일반적인 키워드 검색 도구는 검색어가 포함된 줄을 찾고 하이라이트만 해 주지만, 어떤 대상(예: 파일명, 변수명, 로그 토큰 등)이 키워드를 포함하는지 빠르게 파악하기는 어렵습니다.
이 프로그램은
검색어가 포함된 모든 줄을 출력하면서
마지막에 검색어를 포함하는 고유 단어 목록을 한 번에 보여줍니다.
예를 들어 디렉터리 목록에서 특정 문자열이 포함된 파일명만 목록 형태로 확인하거나, 로그 메시지에서 검색어를 포함한 함수/변수명을 한눈에 모아볼 수 있습니다.
# 사용법
1. make로 빌드함
2. ./hw1 <command> [args...] <search_word>
$ ./hw1 ls -l/bin/zip 하면 첫번째 사진처럼 나온다.
$ ./hw1 grep -n main HW1.c main 찾기나,
$ ./hw1 grep -R error /var/log/syslog error  로그파일에서 에러찾기 같은 기능도 사용할 수 있다.




# Makefile
<img width="321" alt="스크린샷 2025-05-04 오후 3 21 48" src="https://github.com/user-attachments/assets/c0c04a13-e28a-493d-acbd-90600172b6bd" />

사용할 컴파일러 = gcc , 경고,메세지 (1~3줄)
타겟 설정 (4~5줄)
가상의 목표임 표시 (6줄)
빌드시 규칙(7~9줄)
clean(지울 taget 10~11줄)
