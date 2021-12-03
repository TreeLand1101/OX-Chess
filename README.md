# Network-Programming-HW2
# OX-Chess
````
git clone https://github.com/tryyyagain/OX-Chess
cd OX-Chess
make
./server
./client 127.0.0.1 8080
````
- 允許至少2個client同時登錄至server。
- client的使用者可以列出已登入的使用者名單。
- client的使用者可以選擇要跟哪一個使用者下棋，並請求對方的同意。
- 若對方同意後，開始進入棋局。
- 雙方可輪流下棋，直到分出勝負或平手。
- 登入的使用者可選擇登出。

bonus
- list player's winrate
- create own account
- increase password security with md5
- send message to player
- surrender in the game
- handle some invalid input issues
