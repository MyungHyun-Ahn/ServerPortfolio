# RIO Send Dispatch Mode Review

## 1. 紐⑹쟻
- `Backend: Rio`瑜??좎???梨?send 寃쎈줈瑜?`Direct`? `OwnerThread` ??諛⑹떇?쇰줈 ?꾪솚?????덈룄濡??뺤옣???댁슜???뺣━?쒕떎.
- ?대뼡 ?ㅼ젙???대뼡 肄붾뱶 寃쎈줈瑜???붿?, 洹몃━怨??꾩옱 鍮꾧탳 寃곌낵媛 臾댁뾿?몄? ?뺣━?쒕떎.

## 2. ?ㅼ젙 寃쎈줈
?ㅼ젙 ?뚯씪:
- [EchoServer.schema.yaml](D:\Project\ServerPortfolio\RefactoringServer\ConfigSchema\Server\EchoServer.schema.yaml)
- [EchoServer.yaml](D:\Project\ServerPortfolio\RefactoringServer\Config\Server\EchoServer.yaml)

?ㅼ젙 ??

```yaml
EchoServer:
  Backend: Rio
  RioSendDispatchMode: Direct
```

?먮뒗:

```yaml
EchoServer:
  Backend: Rio
  RioSendDispatchMode: OwnerThread
```

留ㅽ븨 寃쎈줈:
1. generated config loader
2. [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Main.cpp)
3. [BackendTypes.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\BackendTypes.h)
4. [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)

## 3. Direct 紐⑤뱶
- ?몄텧???ㅻ젅?쒓? 諛붾줈 `RIOSend()`瑜?嫄대떎.
- cross-thread handoff媛 ?녿떎.
- 媛???⑥닚??pure RIO baseline?대떎.

?먮쫫:
1. app/content thread
2. `FRioServer::SendPacket(...)`
3. `SubmitSendDirect(...)`
4. `RIOSend(...)`
5. owner worker媛 send completion dequeue

## 4. OwnerThread 紐⑤뱶
- `Send()`??owner worker queue??enqueue留??쒕떎.
- ?ㅼ젣 `RIOSend()`??owner worker媛 ?섑뻾?쒕떎.

?먮쫫:
1. app/content thread
2. `FRioServer::SendPacket(...)`
3. `EnqueueOwnerThreadSend(...)`
4. owner worker `DrainSendCommands(...)`
5. `SubmitSendDirect(...)`
6. `RIOSend(...)`
7. owner worker媛 send completion dequeue

## 5. ?꾩옱 ownership ?댁꽍
- recv completion: owner worker
- send completion: owner worker
- `Direct` 紐⑤뱶 send submit: ?몄텧 ?ㅻ젅??- `OwnerThread` 紐⑤뱶 send submit: owner worker

利?
- `Direct`??遺遺?owner-thread 紐⑤뜽
- `OwnerThread`??send hot path源뚯? owner worker濡?紐⑥쑝??鍮꾧탳 紐⑤뜽

## 6. 10遺??뚯씪??寃곌낵
| Mode | responses total | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: |
| Rio Direct | 619436 | 3.584 ms | 8.107 ms | 9.390 ms |
| Rio OwnerThread | 664640 | 34.702 ms | 41.093 ms | 69.929 ms |
| Iocp | 669984 | 4.228 ms | 9.256 ms | 12.053 ms |

?댁꽍:
- ?뚯씪?우뿉?쒕뒗 `OwnerThread`媛 RTT?먯꽌 ?ш쾶 ?먰빐瑜?遊ㅻ떎.
- ??寃곌낵留뚯쑝濡쒕뒗 `OwnerThread`瑜?湲곕낯媛믪쑝濡??щ━湲??대졄?ㅺ퀬 ?먮떒?덈떎.

## 7. 2?쒓컙 蹂몄떎??寃곌낵
| Mode | responses total | avg sendTPS | avg sendBps | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 1768780 | 714.084 | 456290.961 | 3.766% | 1.578 ms | 1.446 ms | 4.647 ms |
| Rio OwnerThread | 1767533 | 711.370 | 454156.445 | 3.729% | 1.470 ms | 1.459 ms | 4.778 ms |
| Iocp | 1767073 | 710.884 | 453679.689 | 3.700% | 1.464 ms | 1.418 ms | 4.651 ms |

?댁꽍:
- `Direct`媛 泥섎━??湲곗??쇰줈 媛??醫뗭븯??
- `OwnerThread`媛 ?뚯씪????蹂댁????댁젏? 2?쒓컙 ?됯퇏?먯꽑 ?좎??섏? ?딆븯??
- RTT????紐⑤뱶媛 鍮꾩듂?섏?留? `OwnerThread`媛 湲곕낯媛믪쑝濡?媛?留뚰겮 紐낇솗???곗꽭?섏쭊 ?딆븯??

## 8. 1?쒓컙 4紐⑤뱶 異붽? 鍮꾧탳
異붽? 鍮꾧탳:
1. `RioDirect` (`SO_SNDBUF=0`)
2. `RioOwnerThread` (`SO_SNDBUF=0`)
3. `IocpSendBuf0`
4. `IocpSendBufDefault`

| Mode | responses total | avg sendTPS | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 884249 | 710.855 | 1.291 ms | 1.504 ms | 4.398 ms |
| Rio OwnerThread | 885203 | 715.116 | 1.195 ms | 1.519 ms | 4.363 ms |
| IocpSendBuf0 | 884536 | 712.994 | 1.228 ms | 1.634 ms | 4.941 ms |
| IocpSendBufDefault | 885876 | 713.353 | 1.116 ms | 1.444 ms | 4.488 ms |

?댁꽍:
- `RIO` ??紐⑤뱶 李⑥씠???묐떎.
- `IOCP`?먯꽌??`SO_SNDBUF=-1`??`0`蹂대떎 ??醫뗪쾶 ?섏솕??
- 利?`RIO`???깅뒫 李⑥씠??`SO_SNDBUF`蹂대떎 send ownership 援ъ“ ?곹뼢?????ш퀬, `IOCP`??`0`??湲곕낯媛믪쑝濡?媛뺤젣???댁쑀媛 ?쏀븯??

## 9. ?꾩옱 寃곕줎
- `RIO`??湲곕낯 send ?뺤콉? `Direct` ?좎?媛 ?곸젅?섎떎.
- `OwnerThread`???꾩냽 理쒖쟻???ㅽ뿕 寃쎈줈濡??④릿??
- `RIO`???꾩옱 援ы쁽?먯꽌 `SO_SNDBUF=0` 湲곕낯?쇰줈 ?щ룄 臾대갑?섎떎.
- `IOCP`??`SO_SNDBUF=0`蹂대떎 湲곕낯媛?`-1`?????リ굅??理쒖냼???먰빐媛 ?녿떎.

## 10. 1?쒓컙 4紐⑤뱶 ?щ퉬援?(`interval=0`, `room-change=10%`)
議곌굔:
- `250 sessions`
- `holdSeconds=3600`
- `interval=0`
- `room-count=80`
- `room-capacity=4`
- `room-change=10%`
- ?쒕쾭? ?대씪?댁뼵?몃? 媛숈? 癒몄떊?먯꽌 ?숈떆 ?ㅽ뻾

寃곌낵:
| Mode | responses total | avg sendTPS | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 20772406 | 7134.390 | 12.068% | 1.133 ms | 1.884 ms | 3.584 ms |
| Rio OwnerThread | 19731937 | 6783.761 | 11.616% | 1.541 ms | 2.490 ms | 4.261 ms |
| IocpSendBuf0 | 20398384 | 6995.189 | 12.106% | 0.927 ms | 1.673 ms | 3.187 ms |
| IocpSendBufDefault | 20591438 | 7085.586 | 12.243% | 1.089 ms | 1.826 ms | 4.033 ms |

?댁꽍:
- `Rio Direct`媛 怨좎븬 議곌굔 泥섎━??湲곗??쇰줈 媛??醫뗭븯??
- `Rio OwnerThread`??媛숈? 怨좎븬 議곌굔?먯꽌 媛??遺덈━??寃곌낵瑜?蹂댁???
- `IocpSendBuf0`??吏?곗? 醫뗭븯吏留?throughput???⑥뼱??湲곕낯媛??꾨낫濡쒕뒗 ?쏀븯??
- `IocpSendBufDefault`??throughput???믨퀬 RTT??臾대궃?댁꽌 `IOCP` 湲곕낯媛믪쑝濡????곸젅?섎떎.

二쇱쓽:
- ?대쾲 ?곗? ?쒕쾭? ?대씪?댁뼵?몃? 媛숈? 癒몄떊?먯꽌 ?숈떆???뚮졇湲??뚮Ц???대씪?댁뼵??CPU ?먯쑀媛 ?쒕쾭 ?섏튂???곹뼢??以ъ쓣 媛?μ꽦???덈떎.
- ?곕씪????寃곌낵???덈? ?깅뒫???꾨땲???곷? ?쒖쐞瑜?蹂대뒗 baseline?쇰줈 ?댁꽍?섎뒗 寃껋씠 留욌떎.



