# NetworkLib Session Send Queue Review

## 1. 踰붿쐞
- [FSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.h)
- [FSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.cpp)
- [FSendBuffer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSendBuffer.h)
- [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)

## 2. 蹂寃?紐⑹쟻
- 媛숈? ?몄뀡??????щ윭 ?ㅻ젅?쒓? ?숈떆??`Send()`瑜??몄텧?대룄 ?ㅼ젣 overlapped `WSASend`???몄뀡??1媛쒕쭔 吏꾪뻾?섍쾶 留뚮뱾 ?꾩슂媛 ?덉뿀??
- ?덇굅???꾨줈?앺듃??`N-Send` ?섎룄泥섎읆, ?≪떊 ?붿껌? ?몄뀡 ?대? ?먯뿉 ?볤퀬 ?ㅼ젣 I/O??諛곗튂濡?臾띠뼱??蹂대궡??援ъ“媛 紐⑺몴???
- ?μ떆媛?寃利앹쓣 ?꾪빐 burst ?뚯뒪?몃쭔???꾨땲?? ?좎? ?쒓컙 ?숈븞 吏?띿쟻?쇰줈 ?붿껌/?묐떟???ㅺ???援ъ“? ?쒕쾭 痢??듦퀎 異쒕젰???꾩슂?덈떎.

## 3. ?ㅺ퀎 ?붿빟
### 3-1. ?몄뀡 ?뚯쑀 ?≪떊 ??- [FSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.h)? `FLockFreeQueue<FSendBuffer*>` 湲곕컲 `m_sendQueue`瑜?媛吏꾨떎.
- [FIocpServer::Send](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)? ?꾨젅?대컢怨??뷀샇?붾? 留덉튇 踰꾪띁瑜?利됱떆 `WSASend` ?섏? ?딄퀬 ?몄뀡 ?먯뿉 ?ｋ뒗??

### 3-2. ?몄뀡???⑥씪 in-flight send
- [FSession::TryBeginSend](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.cpp)? `m_sendInFlight`瑜?`false -> true`濡?CAS ?쒕떎.
- ?대? send I/O媛 吏꾪뻾 以묒씠硫???`WSASend`??嫄몄? ?딄퀬 ?먯뿉留??볦씤??
- send ?꾨즺 ??[FSession::EndSend](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.cpp)濡??뚮옒洹몃? ?대━怨??ㅼ쓬 諛곗튂瑜??ㅼ떆 ?쒕룄?쒕떎.

### 3-3. ?고???寃利?媛??- [FSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.h)??`m_liveSendIoCount`, `m_maxObservedConcurrentSendIoCount`瑜??먯뿀??
- [FIocpServer::PostSend](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)?먯꽌 ?ㅼ젣 `WSASend` 吏곸쟾 `BeginSendIo()`瑜??몄텧?쒕떎.
- ?숈떆??2媛??댁긽 send I/O媛 ?≫엳硫?`Concurrent WSASend detected` 濡쒓렇瑜??④린怨??몄뀡??醫낅즺?쒕떎.
- ?몄뀡 醫낅즺 ??`maxConcurrentSendIo=`瑜?濡쒓렇???④꺼 ?ㅼ젣 愿痢?理쒕?媛믪쓣 ?뺤씤?????덈떎.

### 3-4. Batched WSASend
- [FSession::FillSendBatch](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.cpp)? ?먯뿉??理쒕? `kMaxSendBatchCount`媛쒕? 爰쇰궡 `WSABUF[]`瑜?留뚮뱺??
- [FIocpServer::PostSend](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)? ??諛곗뿴????踰덉쓽 `WSASend`濡??섍릿??

### 3-5. ?μ떆媛??≪닔??寃利앹슜 ?듦퀎
- [IServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)??`GetStatsSnapshot()`??異붽??덈떎.
- [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)???꾨옒瑜??먯옄 移댁슫?곕줈 吏묎퀎?쒕떎.
  - ?꾩쟻 accept ??  - ?쒖꽦 ?몄뀡 ??  - ?섏떊 ?⑦궥 ??  - ?≪떊 ?⑦궥 ??  - `WSARecv` ?몄텧 ??  - `WSASend` ?몄텧 ??- [EchoServer/Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)??`--headless` ?ㅽ뻾 以?1珥덈쭏??`EchoStats`瑜?肄섏넄??異쒕젰?쒕떎.
- ?꾩옱 異쒕젰 ??ぉ:
  - `acceptTPS`
  - `recvTPS`
  - `sendTPS`
  - `wsaRecvTPS`
  - `wsaSendTPS`
  - `totalWSARecvCalls`
  - `totalWSASendCalls`

## 4. ?대씪?댁뼵??寃利?蹂닿컯
- [EchoClient/Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)???댁젣 ???꾨줈?몄뒪?먯꽌 ?щ윭 ?몄뀡???좎??????덈떎.
- 二쇱슂 ?듭뀡:
  - `--sessions`
  - `--hold-seconds`
  - `--interval-ms`
  - `--packets-per-send`
  - `--reconnect-probability-percent`
  - `--reconnect-delay-ms`
- 媛??몄뀡? ?좎? ?쒓컙 ?숈븞 二쇨린?곸쑝濡??붿껌??蹂대궡怨? ?묐떟 吏묓빀 ?꾩껜瑜?寃利앺븳??
- ?곕씪???댁쟾??"burst ??idle"???꾨땲???ㅼ젣濡??μ떆媛?`Send/Recv`媛 諛섎났?섎뒗 寃利앹씠 媛?ν빐議뚮떎.
- `--packets-per-send`???щ윭 ?꾨젅?꾩쓣 ?섎굹??`send()` ?몄텧???댁뼱遺숈뿬 蹂대궡誘濡? ?좏뵆由ъ??댁뀡 ?⑦궥 ?섏? ?뚯폆 send ?몄텧 ?섎? 遺꾨━?댁꽌 蹂????덈떎.
- `--reconnect-probability-percent`??二쇨린 ?ъ씠?대쭏???쇱젙 ?뺣쪧濡??곌껐???딄퀬 ?ㅼ떆 遺숆쾶 ?섎?濡? accept/close 諛섎났 寃쎈줈瑜??μ떆媛?寃利앺븷 ???덈떎.

## 5. ?뺤씤???ъ떎
### 5-1. 湲곕낯 硫?곗뒪?덈뱶 send 寃利?- ?쒕쾭:
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - `--headless --send-thread-count 4 --responses-per-thread 4`
- ?대씪?댁뼵??
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)
  - `--count 8 --payload-size 48 --send-chunk-size 5 --send-chunk-delay-ms 1 --recv-buffer-size 11 --response-thread-count 4 --responses-per-thread 4`
- 寃곌낵:
  - 珥?`128` ?묐떟 ?깃났
  - `echo validation succeeded.` ?뺤씤
  - `Concurrent WSASend detected` 濡쒓렇 誘몃컻??
### 5-2. ??媛뺥븳 burst 寃利?- ?쒕쾭:
  - `--headless --send-thread-count 8 --responses-per-thread 8`
- ?대씪?댁뼵??
  - `--count 16 --payload-size 64 --send-chunk-size 7 --send-chunk-delay-ms 1 --recv-buffer-size 13 --response-thread-count 8 --responses-per-thread 8 --quiet`
- 寃곌낵:
  - 珥?`1024` ?묐떟 ?깃났
  - `echo validation succeeded.` ?뺤씤
  - ?몄뀡 醫낅즺 濡쒓렇?먯꽌 `maxConcurrentSendIo=1` ?뺤씤

### 5-3. 吏???≪닔??寃利?- ?쒕쾭:
  - `--headless --send-thread-count 4 --responses-per-thread 4`
- ?대씪?댁뼵??
  - `--sessions 4 --count 2 --payload-size 32 --send-chunk-size 4 --send-chunk-delay-ms 1 --recv-buffer-size 9 --response-thread-count 4 --responses-per-thread 4 --hold-seconds 3 --interval-ms 500 --quiet`
- 寃곌낵:
  - 珥?`640` ?묐떟 ?깃났
  - ?쒕쾭 肄섏넄???꾨옒 ?듦퀎 異쒕젰 ?뺤씤

```text
[EchoStats] sessions=4 recvTPS=8 sendTPS=128 WSASendCalls=16 WSARecvCalls=81
[EchoStats] sessions=4 recvTPS=12 sendTPS=192 WSASendCalls=52 WSARecvCalls=212
[EchoStats] sessions=4 recvTPS=12 sendTPS=192 WSASendCalls=88 WSARecvCalls=321
```

- 醫낅즺 濡쒓렇?먯꽌???몄뀡蹂?`maxConcurrentSendIo=1` ?뺤씤

### 5-4. ?⑦궥 臾띠쓬 ?≪떊 + ?뺣쪧 ?ъ젒??寃利?- ?쒕쾭:
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - `--headless --send-thread-count 1 --responses-per-thread 1`
- ?대씪?댁뼵??
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)
  - `--sessions 8 --count 8 --payload-size 48 --packets-per-send 4 --send-chunk-size 8 --send-chunk-delay-ms 1 --recv-buffer-size 16 --response-thread-count 1 --responses-per-thread 1 --hold-seconds 3 --interval-ms 500 --reconnect-probability-percent 30 --reconnect-delay-ms 50 --quiet`
- 寃곌낵:
  - 珥?`192` ?묐떟 ?깃났
  - ?뺣쪧 ?ъ젒?띿쑝濡?`acceptTPS`媛 利앷??섎뒗 援ш컙 ?뺤씤
  - ?쒕쾭 異쒕젰 ?덉떆:

```text
[EchoStats] sessions=8 acceptTPS=3 recvTPS=38 sendTPS=38 wsaSendTPS=38 wsaRecvTPS=247 totalWSASendCalls=135 totalWSARecvCalls=941
[EchoStats] sessions=0 acceptTPS=0 recvTPS=57 sendTPS=57 wsaSendTPS=57 wsaRecvTPS=363 totalWSASendCalls=192 totalWSARecvCalls=1304
```

  - `1:1 echo` 紐⑤뱶?먯꽌??`recvTPS`? `sendTPS`媛 媛숆쾶 ?섏삤??寃??뺤씤
  - ?몄뀡 醫낅즺 濡쒓렇?먯꽌 `maxConcurrentSendIo=1` ?좎? ?뺤씤

## 6. ?먮떒
- ?꾩옱 援ъ“??"?щ윭 ?ㅻ젅?쒖쓽 ?숈떆 `Send()` ?몄텧"怨?"?몄뀡???⑥씪 in-flight `WSASend`" 洹쒖튃???묐┰?쒗궎?????깃났?덈떎.
- 寃利?洹쇨굅???⑥닚 ?묐떟 ?깃났留뚯씠 ?꾨땲?? ?ㅼ젣 ?고???媛?쒖? `maxConcurrentSendIo` 怨꾩륫源뚯? ?ы븿?쒕떎.
- 吏???≪닔???곹깭?먯꽌 TPS? `WSASend`/`WSARecv` ?몄텧 ?섎? 肄섏넄濡?愿痢≫븷 ???덉쑝誘濡??μ떆媛??뚯뒪??湲곕컲??留덈젴?먮떎.
- `packets-per-send`? ?뺣쪧 ?ъ젒???듭뀡??異붽??섏뼱, ?⑦궥 諛곗튂 ?≪떊怨?accept/close 諛섎났??媛숈? ?뚯뒪???대씪?댁뼵?몃줈 寃利앺븷 ???덈떎.

## 7. ?⑥? ?뺤씤 ??ぉ
- 100?몄뀡 ?댁긽, 2?쒓컙 ?댁긽 ?μ떆媛??ㅽ뻾 濡쒓렇瑜?湲곗??쇰줈 理쒖쥌 ?덉젙??寃利?- `FSendBuffer`瑜?硫붾え由?? 湲곕컲?쇰줈 諛붽엥???뚯쓽 ?깅뒫 鍮꾧탳
- ?덈Т 湲??≪떊 ?먭? ?볦씪 ?뚯쓽 back-pressure ?뺤콉
