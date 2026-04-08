# ContentsRuntime lock-free inbox ?덉젙??寃利?怨꾪쉷

## 1. 紐⑹쟻
- `packet inbox` lock-free ?꾨줈?좏??낆씠 ?ㅼ젣濡??덉젙?곸씤吏 ?④퀎?곸쑝濡?寃利앺븳??
- contention 媛먯냼留?蹂대뒗 寃껋씠 ?꾨땲?? packet ?좎떎, ?몄뀡 醫낅즺 ?꾨씫, ?μ떆媛?soak ?덉젙?깃퉴吏 ?④퍡 蹂몃떎.

## 2. 寃利????- `FContentThread`??lock-free packet inbox 寃쎈줈
- ?곸슜 ?꾩튂:
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Threading\FContentThread.cpp)
- ?꾩옱 ?좉?:
  - `kUseLockFreePacketInboxPrototype`

## 3. ?좉? ?뺤콉
- ?ㅽ뙣 ??利됱떆 湲곗〈 `deque + mutex` 寃쎈줈濡??섎룎由????덉뼱???쒕떎.
- 鍮꾧탳 湲곗?
  - A: `lock-free = false`
  - B: `lock-free = true`
  - C: `lock-free = true` + race injection on
- ?곸꽭 ?뺤콉? [006_contents-runtime-lockfree-toggle-policy.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\006_contents-runtime-lockfree-toggle-policy.md)瑜?湲곗??쇰줈 留욎텣??

## 4. race injection 寃利??뺤콉
- ?대쾲 寃利앹뿉??`Sleep(0)`, `SwitchToThread()`, `yield()`???깅뒫 理쒖쟻?붽? ?꾨땲??race window ?뺤옣 ?꾧뎄??
- 紐⑹쟻? ?됱냼?먮뒗 ???쒕윭?섏? ?딅뒗 寃쎌웳 ?곹깭瑜????쎄쾶 ?ы쁽?섎뒗 寃껋씠??

### 4.1 沅뚯옣 諛⑹떇
- `Debug` ?먮뒗 寃利??꾩슜 寃쎈줈?먯꽌留??쒖꽦??- 臾댁“嫄?留ㅻ쾲 ?ｌ? 留먭퀬 二쇨린 湲곕컲 ?먮뒗 ?뺣쪧 湲곕컲?쇰줈 ?쎌엯
- ?곗꽑?쒖쐞
  1. `SwitchToThread()`
  2. `Sleep(0)`
  3. `std::this_thread::yield()`

### 4.2 ?쎌엯 ?꾨낫
- `FContentRuntime::EnqueuePacket`
- `FContentRuntime::MoveSession`
- `FContentThread::EnqueuePacket`
- `FContentThread` consumer drain loop

## 5. 寃利??④퀎
### 5.1 湲곕낯 湲곕뒫 寃利?- 紐⑹쟻
  - 理쒖냼 寃쎈줈媛 源⑥?吏 ?딅뒗吏 ?뺤씤
- ?쒕굹由ъ삤
  - `sessions=1`
  - `holdSeconds=0`
- 湲곕?
  - `echo validation succeeded.`

### 5.2 諛섎났 寃쎈줈 寃利?- 紐⑹쟻
  - 媛숈? ?곌껐 ?좎? 以?packet inbox媛 硫덉텛吏 ?딅뒗吏 ?뺤씤
- ?쒕굹由ъ삤
  - `sessions=1`
  - `holdSeconds=1`
  - `intervalMs=200`
- 湲곕?
  - `EchoRq/EchoRp` 諛섎났 泥섎━
  - ?뺤긽 醫낅즺

### 5.3 ?ㅼ쨷 ?몄뀡 寃利?- 紐⑹쟻
  - ?щ윭 producer媛 ?숈떆??enqueue?대룄 packet ?좎떎???녿뒗吏 ?뺤씤
- ?쒕굹由ъ삤
  - `sessions=8~100`
  - `packetsPerSend=2~4`
- 湲곕?
  - ?묐떟 ?좎떎 ?놁쓬
  - ?몄뀡 ?꾩닔 ?놁쓬

### 5.4 contention 寃利?- 紐⑹쟻
  - ?ㅼ젣濡?lock wait媛 以꾩뿀?붿? ?뺤씤
- ?쒕굹由ъ삤
  - `sessions=100`
  - `holdSeconds=3~5`
  - `intervalMs=0`
  - `packetsPerSend=4`
- ?뺤씤 ??ぉ
  - `runtimeEnqueueLockUs`
  - `echoPacketEnqueueLockUs`
  - `enqueueFailTPS`

### 5.5 race injection 寃利?- 紐⑹쟻
  - ?⑥뼱 ?덈뒗 寃쎌웳 ?곹깭瑜??쒕윭?몃떎
- ?쒕굹由ъ삤
  - `sessions=8~100`
  - race injection on
- 湲곕?
  - deadlock ?놁쓬
  - packet ?좎떎 ?놁쓬
  - ?몄뀡 醫낅즺 ?꾨씫 ?놁쓬

### 5.6 ?μ떆媛?寃利?- 紐⑹쟻
  - soak ?덉젙???뺤씤
- ?쒕굹由ъ삤
  - `DurationSeconds=7200`
  - `sessions=100`
  - race injection on
- 湲곕?
  - ?몄뀡 ?꾩닔 ?놁쓬
  - 鍮꾩젙??醫낅즺 ?놁쓬
  - `enqueueFailTPS=0`

## 6. ?ㅽ뙣 湲곗?
- `echo validation succeeded.` 誘몄텧??- `enqueueFailTPS > 0`
- ?몄뀡 醫낅즺 ?꾨씫
- ?꾨줈?몄뒪媛 ?쒓컙 ???뺤긽 醫낅즺?섏? 紐삵븿
- bootstrap ?④퀎 timeout

## 7. 理쒖쥌 ?먮떒 湲곗?
- 湲곕낯/諛섎났/?ㅼ쨷 ?몄뀡 寃利??듦낵
- contention 吏??媛쒖꽑
- race injection 寃利??듦낵
- ?μ떆媛?soak 寃利??듦낵

## 8. TODO
- ?쇰컲 `Out\\EchoServer.exe` 湲곗? ?μ떆媛?寃곌낵 蹂꾨룄 臾몄꽌??- ?꾩슂?섎㈃ `2 hour`? `8 hour`瑜?遺꾨━ ?댁쁺

