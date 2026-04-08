# IOCP AcceptEx ?꾪솚 ?뺣━

## 1. 紐⑹쟻
- `FIocpServer`??湲곗〈 `accept()` 湲곕컲 accept path瑜??덇굅??湲곗???留욎떠 `AcceptEx + IOCP completion` 援ъ“濡??꾪솚?쒕떎.
- ?꾪솚 ?꾩뿉??湲곗〈 `recv/send`, `ContentsRuntime`, `EchoServer` ?먮쫫? ?좎??쒕떎.
- ?꾪솚 寃곌낵? ?⑥? ?먮떒 ?ы빆???④퍡 ?뺣━?쒕떎.

## 2. ?꾩옱 ?곹깭
- ?곹깭: ?꾨즺
- 肄붾뱶
  - [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.h)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)
- 寃利?  - `1 session / 5 sec` ?ㅻえ???깃났
  - `100 sessions / 3 min` ?뚭? ?깃났
  - 怨좊퉰???ъ젒??stress 寃利??깃났

## 3. 援ы쁽 寃곌낵
### 3-1. accept thread ?쒓굅
- 蹂꾨룄 blocking `accept()` thread瑜??쒓굅?덈떎.
- listen socket accept??worker??`GetQueuedCompletionStatus()` completion plane?먯꽌 泥섎━?쒕떎.

### 3-2. AcceptEx ?ㅼ쨷 pre-post
- `LoadAcceptExFunctions()`濡?`AcceptEx`, `GetAcceptExSockaddrs`瑜?濡쒕뱶?쒕떎.
- `InitializeAcceptContexts()`?먯꽌 怨좎젙 湲몄씠 `SAcceptContext[]`瑜?留뚮뱾怨?媛?slot??`PostAccept(slotIndex)`瑜?嫄몄뼱?붾떎.
- accept completion???ㅻ㈃ `HandleAcceptCompletion(...)` ?댄썑 媛숈? slot???ㅼ떆 `PostAccept(slotIndex)`瑜?嫄대떎.

### 3-3. attach ?쒖꽌
- accept completion ??attach ?쒖꽌???ㅼ쓬怨?媛숇떎.
1. `SO_UPDATE_ACCEPT_CONTEXT`
2. `SO_SNDBUF` ?듭뀡 ?곸슜 ?꾩슂 ???곸슜
3. free session slot ?먯깋
4. `FIocpSession::Create()`
5. `FIocpSession::Initialize(...)`
6. `CreateIoCompletionPort(clientSocket, m_iocpHandle, ...)`
7. `OnClientConnected(sessionId)`
8. `PostRecv(*sessionContext)`

### 3-4. ?ㅽ뙣 蹂듦뎄
- attach ?ㅽ뙣 ??accepted socket???リ퀬 媛숈? slot???ㅼ떆 `AcceptEx`瑜?repost ?쒕떎.
- `AcceptEx completion failed`, `AcceptEx repost failed`, `SO_UPDATE_ACCEPT_CONTEXT failed`瑜?蹂꾨룄 濡쒓렇濡??④릿??

## 4. ?덇굅?쒖? 留욎텣 ?섎?
- ?꾩쟾??媛숈? 硫붾え由?援ъ“瑜???릿 寃껋? ?꾨땲??
- ????덇굅?쒖뿉??以묒슂?덈뜕 ?섎???留욎톬??
  - accept瑜?completion plane?먯꽌 泥섎━
  - pending accept瑜??щ윭 媛?誘몃━ 寃뚯떆
  - accept ?꾨즺 吏곹썑 諛붾줈 ?ㅼ쓬 accept ?ш쾶??  - attach ?꾩뿉 `SO_UPDATE_ACCEPT_CONTEXT` ?곸슜

## 5. ?꾩옱 accept slot pool ?곹깭
### 5-1. ?곸슜??寃?- ?꾩옱 `FIocpServer`?먮뒗 `SAcceptContext[]` 湲곕컲 accept slot pool???덈떎.
- slot?먮뒗 ?ㅼ쓬 ?뺣낫媛 ?ㅼ뼱媛꾨떎.
  - `OVERLAPPED`
  - `acceptedSocket`
  - `slotIndex`
  - accept address buffer

### 5-2. ?꾩쭅 ?곸슜?섏? ?딆? 寃?- `acceptedSocket` ?먯껜???ъ궗?⑺븯吏 ?딅뒗??
- `PostAccept()` ?몄텧留덈떎 ?댁쟾 socket???リ퀬 ??`WSASocketW(...)`瑜?留뚮뱺??
- 利??꾩옱 援ъ“??`accept context slot pool`?댁?, `accepted socket reuse pool`? ?꾨땲??

## 6. ?ъ젒??stress 寃利?寃곌낵
- ?곸꽭 由щ럭: [017_iocp-acceptex-reconnect-stress-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\017_iocp-acceptex-reconnect-stress-review.md)
- ?듭떖 寃곌낵
  - `10 sessions / 3 min / reconnect 100% / delay 250ms` ?깃났
  - ?쒕쾭 痢?`AcceptEx completion failed`, `AcceptEx repost failed`, `SO_UPDATE_ACCEPT_CONTEXT failed`, `All session slots are in use` 紐⑤몢 `0嫄?
  - `Client connected | Session closed` ?⑷퀎 `20100`
- ??怨듦꺽?곸씤 議곌굔?먯꽌 癒쇱? ?곗쭊 寃껋? `AcceptEx` ?쒕쾭 寃쎈줈媛 ?꾨땲??  - room ?섏슜??遺議?  - 濡쒖뺄 ?대씪?댁뼵??ephemeral port ?쒓퀎(`10048`)
???

## 7. socket ?ъ궗???먮떒
- `accepted socket reuse`???꾩옱 湲곕낯 ?곸슜?섏? ?딅뒗??
- ?댁쑀
  - ?ㅼ륫 湲곗? 李⑥씠媛 ?ъ? ?딆븯??
  - accept ?댄썑 蹂묐ぉ? 蹂댄넻 socket ?앹꽦/?뚭눼蹂대떎 attach, 泥?recv, bootstrap, send 履쎌뿉?????ш쾶 ?섑??쒕떎.
  - socket ?ъ궗?⑹? ?곹깭 珥덇린?? 誘몄셿猷?I/O ?뺣━, attach ?쒖꽌 愿由ш? 源뚮떎濡?떎.
- ?꾩옱 ?먮떒
  - 湲곕낯媛믪쑝濡쒕뒗 鍮꾪솢???좎?
  - ?꾩슂 ??蹂꾨룄 ?ㅽ뿕 ?듭뀡?쇰줈留??ш???
## 8. ?⑥? ?꾩냽 ?묒뾽
- `IOCP AcceptEx` ?꾪솚 ??accept path ?깅뒫 鍮꾧탳
- ?꾩슂 ??`accepted socket reuse` ?ㅽ뿕 ?듭뀡 ?щ룄??- ?꾩슂 ??`AcceptExCount`瑜?config濡??몄텧

