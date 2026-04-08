# IOCP AcceptEx Reconnect Stress Review

## 1. 紐⑹쟻
- `FIocpServer`??`AcceptEx` ?꾪솚 ?댄썑, ?ъ젒??churn?????곹솴?먯꽌??accept path媛 ?덉젙?곸쑝濡??좎??섎뒗吏 ?뺤씤?쒕떎.
- ?ㅽ뙣媛 諛쒖깮?섎뜑?쇰룄 ?먯씤??`AcceptEx` ?쒕쾭 寃쎈줈?몄?, ?꾨땲硫?濡쒖뺄 ?뚯뒪???섍꼍/?대씪?댁뼵???쒓퀎?몄? 遺꾨━?쒕떎.
- ?꾩옱 援ы쁽??accept slot pool???ㅼ젣濡??곸슜?섏뼱 ?덈뒗吏, 洹몃━怨??덇굅?쒖쓽 accept session pool怨??대뼡 李⑥씠媛 ?덈뒗吏 ?뺣━?쒕떎.

## 2. ????뚯씪
- IOCP backend
  - [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.h)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)
- 湲곗? ?먮쫫 臾몄꽌
  - [016_iocp-echo-server-flow-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\016_iocp-echo-server-flow-review.md)

## 3. ?뚯뒪???쒕굹由ъ삤
### 3-1. ?ㅽ뙣 ?쒕굹由ъ삤 1
- 寃쎈줈: [iocp_acceptex_reconnect_100x3m](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_acceptex_reconnect_100x3m)
- 議곌굔
  - `100 sessions`
  - `3 minutes`
  - `reconnectProbabilityPercent=100`
  - `reconnectDelayMs=0`
  - `roomCount=20`
- 寃곌낵
  - ?대씪?댁뼵???ㅽ뙣: `session[0] failed: no joinable room available.`
- ?댁꽍
  - ??耳?댁뒪??accept path 臾몄젣媛 ?꾨땲??room ?섏슜??遺議깆씠??

### 3-2. ?ㅽ뙣 ?쒕굹由ъ삤 2
- 寃쎈줈: [iocp_acceptex_reconnect_100x3m_room50](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_acceptex_reconnect_100x3m_room50)
- 議곌굔
  - `100 sessions`
  - `3 minutes`
  - `reconnectProbabilityPercent=100`
  - `reconnectDelayMs=0`
  - `roomCount=50`
- 寃곌낵
  - ?대씪?댁뼵???ㅽ뙣: `session[0] failed: connect failed: 10048`
- ?댁꽍
  - ?쒕쾭 accept path蹂대떎 ?대씪?댁뼵??濡쒖뺄 ?ы듃 ?ъ궗???쒓퀎媛 癒쇱? ?곗쭊 耳?댁뒪??
  - ?쒕쾭 濡쒓렇?먮뒗 `AcceptEx completion failed`, `AcceptEx repost failed`, `SO_UPDATE_ACCEPT_CONTEXT failed`, `All session slots are in use`媛 ?섑??섏? ?딆븯??

### 3-3. ?ㅽ뙣 ?쒕굹由ъ삤 3
- 寃쎈줈: [iocp_acceptex_reconnect_50x3m_delay10](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_acceptex_reconnect_50x3m_delay10)
- 議곌굔
  - `50 sessions`
  - `3 minutes`
  - `reconnectProbabilityPercent=100`
  - `reconnectDelayMs=10`
- 寃곌낵
  - ?대씪?댁뼵???ㅽ뙣: `session[0] failed: connect failed: 10048`
- ?댁꽍
  - delay瑜?`10ms`濡??섎젮??濡쒖뺄 ?대씪?댁뼵???ы듃 ?쒓퀎媛 癒쇱? ?쒕윭?щ떎.

### 3-4. ?좏슚??怨좊퉰???ъ젒??寃利?- 寃쎈줈: [iocp_acceptex_reconnect_10x3m_delay250](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_acceptex_reconnect_10x3m_delay250)
- 議곌굔
  - `10 sessions`
  - `3 minutes`
  - `reconnectProbabilityPercent=100`
  - `reconnectDelayMs=250`
  - `roomCount=20`
- 寃곌낵
  - ?대씪?댁뼵???깃났
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_acceptex_reconnect_10x3m_delay250\client.log)
    - `echo validation succeeded. sessions=10 responses=6700 ...`
  - ?쒕쾭 濡쒓렇 吏묎퀎
    - `AcceptEx completion failed`: `0`
    - `AcceptEx repost failed`: `0`
    - `SO_UPDATE_ACCEPT_CONTEXT failed`: `0`
    - `All session slots are in use`: `0`
    - `Client connected | Session closed` ?⑷퀎: `20100`
- ?댁꽍
  - `AcceptEx` 寃쎈줈??吏㏃? 二쇨린??connect/disconnect churn???뺤긽?곸쑝濡?泥섎━?덈떎.

## 4. ?꾩옱 accept slot pool ?곸슜 ?щ?
### 4-1. 寃곕줎
- ?꾩옱 `FIocpServer`?먮뒗 accept slot pool???곸슜?섏뼱 ?덈떎.
- ?ㅻ쭔 ???? `accept context` ?ъ궗????닿퀬, ?덇굅?쒖뿉??蹂대뜕 ?섎???`accepted socket源뚯? ?ъ궗?⑺븯??accept session pool`? ?꾨땲??

### 4-2. ?곸슜??寃?- `SAcceptContext[]` 怨좎젙 諛곗뿴
- slot蹂?`OVERLAPPED`, address buffer, `acceptedSocket`, `slotIndex`
- ?쒕쾭 ?쒖옉 ??pre-post
- accept completion ??媛숈? slot??repost

### 4-3. ?꾩쭅 ?곸슜?섏? ?딆? 寃?- `acceptedSocket` ?먯껜???ъ궗??- ?꾩옱 `PostAccept()`??留ㅻ쾲 ??`WSASocketW(...)`瑜?留뚮뱾怨??댁쟾 socket? ?ル뒗??

## 5. socket ?ъ궗???먮떒
- ?꾩옱 ?먮떒? `湲곕낯 梨꾪깮?섏? ?딆쓬`?대떎.
- ?댁쑀
  - 吏곸젒 ?뚯뒪??湲곗? 李⑥씠媛 ?ъ? ?딆븯??
  - accept ?댄썑 蹂묐ぉ? 蹂댄넻 attach, bootstrap, send 寃쎈줈?먯꽌 ???ш쾶 蹂댁씤??
  - socket ?ъ궗?⑹? ?곹깭 珥덇린?붿? 誘몄셿猷?I/O ?뺣━媛 源뚮떎濡?떎.
- 寃곕줎
  - 吏湲덉? `accept context slot pool`留??좎?
  - `accepted socket reuse`???꾩슂 ??蹂꾨룄 ?듭뀡 ?ㅽ뿕?쇰줈留??ш???
## 6. 寃곕줎
- `AcceptEx` ?꾪솚 ???ъ젒?띿씠 ??? ?곹솴?먯꽌???쒕쾭 accept path???뺤긽 ?숈옉?덈떎.
- ??怨듦꺽?곸씤 議곌굔?먯꽌 癒쇱? ?곗쭊 寃껋? `AcceptEx`媛 ?꾨땲??濡쒖뺄 ?대씪?댁뼵???ы듃 ?ъ궗???쒓퀎???
- ?꾩옱 `FIocpServer`??accept slot pool???대? 媛뽮퀬 ?덉?留? 洹?踰붿쐞??`accept context` ?ъ궗?⑷퉴吏??

