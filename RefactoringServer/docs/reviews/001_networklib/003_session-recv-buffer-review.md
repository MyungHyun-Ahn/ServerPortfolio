# NetworkLib Session Recv Buffer Review

## 1. 踰붿쐞
- [`FRecvBuffer.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FRecvBuffer.h)
- [`FPacketView.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketView.h)
- [`IPacketFramer.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\IPacketFramer.h)
- [`FDefaultPacketFramer.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)
- [`FSession.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.h)
- [`FSession.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.cpp)
- [`FIocpServer.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)
- [`IApplicationHandler.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)

## 2. 蹂寃?紐⑹쟻
- ?댁쟾 recv 寃쎈줈??`WSARecv -> ?꾩떆 vector -> ?몄뀡 ?꾩쟻 vector` ?쒖꽌濡???踰???蹂듭궗?덈떎.
- ?덇굅???꾨줈?앺듃??媛뺤젏? ?몄뀡??recv ring buffer瑜?吏곸젒 ?뚯쑀?섍퀬, `WSARecv`媛 洹?free ?곸뿭?쇰줈 諛붾줈 ?ㅼ뼱媛꾨떎???먯씠?덈떎.
- ?대쾲 蹂寃쎌? 洹??섎룄瑜??꾩옱 `PacketFramer` 援ъ“??留욊쾶 ??린怨? 媛?ν븯硫?payload瑜?蹂꾨룄 踰꾪띁濡?蹂듭궗?섏? ?딄퀬 view濡??섍린??寃껋씠 紐⑹쟻?대떎.

## 3. ?ㅺ퀎 ?붿빟
### 3-1. ?몄뀡 ?뚯쑀 recv ring buffer
- [`FSession`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.h)? [`FRecvBuffer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FRecvBuffer.h)瑜??뚯쑀?쒕떎.
- `FRecvBuffer`??`readOffset`, `writeOffset`, `usedSize` 湲곕컲??怨좎젙 ?⑸웾 ring buffer??
- `BuildRecvWsabufs()`??free ?곸뿭??理쒕? 2媛?`WSABUF`濡??섎닠 `WSARecv`???섍릿??

### 3-2. direct recv
- [`FIocpServer::PostRecv()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)? ???댁긽 ?꾩떆 recv vector瑜?留뚮뱾吏 ?딅뒗??
- ?몄뀡 recv ring buffer free ?곸뿭??諛붾줈 `WSARecv` ???踰꾪띁濡??ъ슜?쒕떎.
- ?꾨즺 ??[`FSession::CommitRecvBytes()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FSession.cpp)濡??ㅼ젣 ?섏떊 湲몄씠留?諛섏쁺?쒕떎.

### 3-3. framer??recv buffer 吏??- [`IPacketFramer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\IPacketFramer.h)??`std::vector<char>`肉??꾨땲??[`FRecvBuffer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FRecvBuffer.h)?먯꽌???⑦궥??異붿텧?????덇쾶 ?먮떎.
- [`FDefaultPacketFramer`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)??recv buffer?먯꽌 ?ㅻ뜑瑜?`Peek`?섍퀬, ?꾩슂?섎㈃ `EnsureContiguous()`濡??꾩옱 ?⑦궥 援ш컙留??좏삎?뷀븳??

### 3-4. packet view 湲곕컲 dispatch
- [`FPacketView`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FPacketView.h)??`opcode`, `randomKey`, `checkSum`, `payload pointer`, `payloadLength`瑜??대뒗 ?뉗? view ??낆씠??
- [`FDefaultPacketFramer::TryExtractPacketView()`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)??payload瑜?蹂꾨룄 `std::vector<char>`濡?蹂듭궗?섏? ?딄퀬 recv buffer ?대? ?ъ씤?곕? view濡?留뚮뱺??
- [`IApplicationHandler`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)???댁젣 `const Packet::FPacketView&`瑜?諛쏆븘, ?꾩옱 肄쒕갚 踰붿쐞 ?덉뿉?쒕쭔 ?좏슚??payload view瑜??ъ슜?쒕떎.
- 肄쒕갚???앸궃 ?ㅼ뿉??[`FIocpServer.cpp`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)媛 `Discard()`濡??대떦 ?⑦궥 湲몄씠留뚰겮 recv buffer瑜??꾩쭊?쒗궓??

## 4. ?먮떒 洹쇨굅
- ?덇굅??[`CNetSession::PostRecv()`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp), [`CNetSession::RecvCompleted()`](D:\Project\ServerPortfolio\NetworkLib\CNetServer.cpp)??ring buffer direct recv? view 吏???ㅺ퀎?쇰뒗 ?먯뿉???ъ쟾??李멸퀬 媛移섍? ?덉뿀??
- ?ㅻ쭔 ?덇굅?쒖쿂???몄뀡??泥댄겕?? ?붾났?명솕, 肄섑뀗痢??먭퉴吏 ?꾨? ?뚯뼱?덉쑝硫??꾩옱 怨꾩링 遺꾨━ 諛⑺뼢怨?異⑸룎?쒕떎.
- 洹몃옒???대쾲?먮뒗 "recv ring buffer? payload view"源뚯?留??몄뀡/?⑦궥 怨꾩링??媛?몄삤怨? ?댁꽍怨?dispatch 寃쎄퀎??framer? application handler???④꺼???

## 5. ?뺤씤???ъ떎
- [`LockFreeTests.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)?먯꽌 ?꾨옒 ??ぉ PASS
  - `Packet framer recv buffer`
  - `Packet framer packet view`
- 湲곗〈 ?뚯뒪?몃룄 ?꾨? PASS
  - `Queue linear FIFO`
  - `Queue parallel sum`
  - `Stack parallel sum`
  - `TLS memory pool parallel`
  - `Packet cipher round trip`
  - `Null packet cipher`
  - `Packet framer round trip`
  - `Packet framer partial receive`
- [`EchoServer.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe) `--headless` + [`EchoClient.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)
  - `--count 8`
  - `--payload-size 48`
  - `--send-chunk-size 5`
  - `--send-chunk-delay-ms 1`
  - `--recv-buffer-size 11`
  - ??議곌굔?먯꽌 `echo validation succeeded.` ?뺤씤

## 6. ?꾩옱 ?쒓퀎
- recv ring buffer ?⑸웾? ?꾩옱 `max(recvBufferSize * 8, 65536)` 湲곗??대떎. ?꾩떆 湲곗??대?濡??댁쁺 湲곗? ?⑸웾? ?댄썑 議곗젙???꾩슂?섎떎.
- `PacketFramer`媛 ?녿뒗 寃쎈줈???꾩옱 ring buffer recv?먯꽌 吏?먰븯吏 ?딅뒗?? 吏湲?援ъ“??framer 湲곕컲 ?쒕쾭瑜??꾩젣濡??쒕떎.
- payload媛 wrap??寃쎌슦 `EnsureContiguous()`媛 ?꾩옱 ?ъ슜 以??곗씠???쇰?瑜?踰꾪띁 ?대??먯꽌 ?좏삎?뷀븳?? 利?蹂꾨룄 payload 踰꾪띁 蹂듭궗???놁뼱議뚯?留? wrap ?곹솴???대? ?뺣젹 蹂듭궗???ъ쟾??議댁옱?????덈떎.
- `FPacketView`??肄쒕갚 踰붿쐞 ?덉뿉?쒕쭔 ?좏슚?섎떎. 肄섑뀗痢?怨꾩링???ㅻ옒 蹂닿??섎젮硫?吏곸젒 蹂듭궗?댁빞 ?쒕떎.

## 7. ?ㅼ쓬 ?뺤씤 ??ぉ
- packet dispatcher 怨꾩링???ㅼ뼱?붿쓣 ??`FPacketView`瑜?洹몃?濡??섍만吏, 蹂꾨룄 content header view瑜??섏? 寃??- ?몄뀡 醫낅즺 吏곸쟾 recv overflow, malformed packet ?곹솴 異붽? ?뚯뒪??- ?μ떆媛?soak 議곌굔?먯꽌 recv ring buffer 寃쎈줈 寃利?