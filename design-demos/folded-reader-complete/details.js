const legacyDraw=draw, core=['home','shelf','files','settings','reader','contents','type'];
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const actionIcons = {
 'stats:all':'chart-column', 'stats:book':'chart-column', quickWifi:'wifi', quickBt:'bluetooth',
 refreshNow:'refresh-cw', lock:'lock', 'edit:search':'search', 'edit:folder':'folder',
 'dest:copy':'copy', 'dest:move':'folder-input', 'ask:delFile':'trash', dirOpen:'folder-open',
 rescan:'refresh-cw', dir:'folder', dirOps:'folder', file:'file'
};
function button(s,a,cl='row') {
 const [action, target] = a.split(':');
 const icon = action==='open' ? routeIcons[target] : actionIcons[a] || actionIcons[action];
 if (icon && (cl==='row' || cl==='chip')) {
   s = s.startsWith('<b>') ? s.replace(/<b>(.*?)<\/b>/, (_, label)=>'<b>'+iconLabel(icon,label)+'</b>') : iconLabel(icon,s);
 }
 return '<button class="'+cl+'" data-act="'+esc(a)+'">'+s+'</button>';
}
const link=(s,v,end='›')=>button('<b>'+s+'</b><span>'+end+'</span>','open:'+v);
const choice=(k,arr)=>'<div class="subnav value-choices">'+arr.map(x=>button(esc(x),'set:'+k+':'+x,pref[k]===x?'chosen chip':'chip')).join('')+'</div>';
const section=(s,html)=>'<div class="group">'+s+'</div>'+html;
const hint=s=>'<p class="help">'+s+'</p>';
const title=s=>'<h1>'+s+'</h1>';
const prefs={sort:'按最近',source:'全部',layout:'册序列表',mode:'左右翻页',font:'内置宋体',systemFont:'系统黑体',contrast:'标准',scale:'100%',wave:'普通',homeWave:'普通',homeClean:'关闭',indent:'2 字',margin:'标准',paragraph:'标准',line:'关闭',shake:'关闭',power:'关闭',immersive:'关闭',pictures:'显示',snap:'10 页',auto:'10 分钟',sleep:'浅睡后深睡',style:'阅读票根',name:'折角读者',sign:'',time:'08:42',date:'2026-11-26',wifi:'未连接',bt:'关闭',learn:'未学习',preset:'默认',fav:[],hidden:[],removed:[],aliases:{},added:[],shelfDir:'',folders:['散文','旅行','待读'],fileFolder:{0:'/books/散文',1:'/books/散文',4:'/books/旅行'},wifiSaved:[],pin:'',wall:'河纹',avatar:'字印'};
let pref={...prefs};try{if(!demo)Object.assign(pref,JSON.parse(localStorage.getItem('folded-details-v1')||'{}'))}catch{}
const store=()=>{try{if(!demo)localStorage.setItem('folded-details-v1',JSON.stringify(pref))}catch{}};
let trail=[],notice='',selected=[],query='',dir='/',file='',dest='',job='',edit='',buf='',ime='拼音',composition='',candPage=0,caret=0,upper=false,code='',pinStage='',firstCode='',network='阅览室',lockFrom='home',netmode='Wi-Fi',server=false,usb=false,clip='',updateMode='联网 OTA',checked=false,learnKey='下一页',keyslot='中键短按',statusKey='no-card',statsBook=false;
for(const [name,chapter,para] of [['山窗小录','第一场霜','山窗向北开。清晨推开木框，第一场霜已经落在松针上。'],['沿海慢车旅行札记','海边小站','傍晚的慢车沿着海岸往南。窗边的座位空着，桌上的茶水微微晃动。']]){let j=books.length;books.push({name,type:'TXT',chs:[chapter],pages:[[para,'站在窗前，远处的声音一点点清晰起来。']]});starts.push([0]);state.pos[j]??=0;if(!state.recent.includes(j))state.recent.push(j)}
const currentName=j=>pref.aliases[j]||books[j].name;
const ids=()=>books.map((_,i)=>i).filter(j=>!pref.removed.includes(j));
const sourceMatch=j=>pref.source==='全部'||pref.source==='TF 卡'||(pref.source==='收藏'?pref.fav.includes(j):books[j].type===pref.source);
let pendingShelfDir = '';
const directoryPaths = () => ['/books', ...pref.folders.map(name => '/books/' + name)];
const directoryName = path => path ? path.split('/').pop() : '全部藏书';
function shelfIds(path = pref.shelfDir) {
  return ids().filter(id => !pref.hidden.includes(id) && (!path ||
    (pref.fileFolder[id] || '/books') === path ||
    pref.added.some(copy => copy.dir === path && copy.book === id)));
}
function setShelfDirectory(path) {
  pref.shelfDir = directoryPaths().includes(path) ? path : '';
  focusPage = 0;
  selected = [];
  query = '';
  trail = [];
  store();
  go('shelf');
}
function shelfScope() {
  const path = pref.shelfDir;
  return '<div class="scope">'+button('<span><small>书架目录</small><b>'+esc(directoryName(path))+'</b></span><span>更换 ›</span>','pickDirectory','scope-pick')+
    (path ? '<div class="scope-path"><code>'+esc(path)+'</code>'+button('恢复默认','resetScope','')+'</div>' : '')+'</div>';
}
function directoryPicker() {
  const paths = ['', ...directoryPaths()];
  return title('选择目录')+'<p class="help">选择书架要显示的藏书范围。</p>'+
    paths.map((path,index)=>{
      const checked = path === pendingShelfDir;
      const count = shelfIds(path).length;
      return '<button class="dir-option'+(checked?' chosen':'')+'" aria-pressed="'+checked+'" data-act="chooseScope:'+encodeURIComponent(path)+'"><span class="mono">'+(index?String(index).padStart(2,'0'):'—')+'</span><span><b>'+esc(path?directoryName(path):'默认书架')+'</b><small>'+esc(path||'所有目录')+'</small></span><span>'+count+' 册 '+(checked?'✓':'')+'</span></button>';
    }).join('');
}
function push(v){if(view!==v)trail.push(view);view=v;panel='';notice='';draw()}
function pop(){view=trail.pop()||'home';panel='';notice='';draw()}
function msg(s){notice=s;draw()}
const backBtn=()=>button('‹ 返回','back','');
function shell(t,html,foot=backBtn()){root.innerHTML='<header>'+button('‹　'+t,'back','')+button(esc(pref.time)+'　▰ 82%','open:quick','')+'</header><article>'+html+'</article><footer>'+foot+'</footer>'}
const selectRows=(k,vals)=>vals.map(v=>button('<span>'+esc(v)+'</span><b>'+(pref[k]===v?'✓':'')+'</b>','set:'+k+':'+v)).join('');
const inputs={search:'搜索书名',rename:'重命名',folder:'新建文件夹',name:'读者姓名',sign:'状态栏签名',password:'网络口令',date:'日期',time:'时间'};
const statuses={
 'no-card':['未检测到 TF 卡','插入 FAT16 或 FAT32 存储卡，再检查卡槽。','files','重新检查'],
 'unmounted':['存储卡无法挂载','卡已插入，文件系统无法读取。可先用电脑备份资料。','storage','查看存储卡'],
 'busy':['存储卡正由电脑使用','先在电脑安全弹出磁盘，再结束 USB 读卡。','transferHub','查看传输'],
 'empty':['书架暂时为空','将 EPUB 或 TXT 放入 /books，再扫描书架。','manager','管理书架'],
 'missing':['找不到原书','上次记录指向 /books/河岸手记.epub。请确认原路径。','files','查看文件'],
 'bad-book':['无法打开书籍','书籍结构无法读取。请核对原文件并重新传入。','files','返回文件'],
 'font-error':['字体加载失败','请检查 /fonts 中的 TTF 文件，或改用内置字体。','typography','选择字体'],
 'collision':['已有同名文件','保留已有文件，改用另一个名称。','fileOps','返回文件操作'],
 'space':['可用空间不足','当前传输已停止。检查卡内剩余空间，再重试。','storage','查看空间'],
 'wifi-error':['连接失败','请检查口令与信号，重新选择网络。','wireless','重新连接'],
 'no-results':['没有匹配的书','清除搜索词或切换来源后重新查找。','manager','查看筛选'],
 'ble-error':['翻页器连接中断','唤醒翻页器并将其置于配对模式。','bluetooth','重新扫描'],
 'ota-error':['升级校验失败','当前可用固件仍可启动。检查网络或重新复制升级包。','updates','返回系统升级'],
 'no-package':['未找到升级包','请将 Pico-update.bin 放在 TF 卡根目录。','updates','重新检查'],
 'backup-error':['未找到备份','确认备份文件位于卡根目录，再检查卡状态。','backup','返回备份'],
 'recovery':['上次打开此书时中断','河岸手记。自动续读已暂停，可以先回到首页。','home','返回首页'],
 'bad-pin':['密码不正确','重新输入四位数字。','unlock','重新输入'],
 'transfer-error':['传输已中断','保留已完成的文件。检查连接后重新发送未完成的文件。','transferHub','查看连接'],
 'no-cover':['暂无封面','使用书名作为封面，阅读进度照常显示。','shelf','查看书架'],
 'low-power':['电量不足','连接稳定电源后再开始升级。','updates','返回升级']
};
const groups={
 appearance:['显示与个性化',[['系统字体','systemFont','系统黑体|内置宋体|卡内 TTF'],['系统字号','scale','100%|125%|150%|175%|200%'],['文字对比','contrast','柔和|标准|清晰|鲜明|高对比'],['主页刷新','homeWave','普通|快刷|水波纹'],['主页全刷','homeClean','开启|关闭'],['书架样式','layout','册序列表|深色书轨|亚克力书架']]],
 typography:['字体与排版',[['阅读字体','font','内置宋体|系统黑体|卡内 TTF'],['左右边距','margin','窄|标准|宽'],['段间距','paragraph','紧凑|标准|宽松'],['首行缩进','indent','0 字|1 字|2 字'],['缩进微调','fine','−1|0|+1'],['文字抗锯齿','aa','开启|关闭'],['阅读辅助线','line','关闭|虚线|点线']]],
 controls:['翻页与按键',[['触摸翻页','mode','左右翻页|上下翻页|仅滑动'],['章节翻页效果','wave','普通|水波纹'],['自动全刷','snap','5 页|10 页|15 页|30 页|不强制'],['电源键翻页','power','关闭|开启'],['摇动翻页','shake','关闭|开启'],['全屏沉浸','immersive','关闭|开启'],['书内图片','pictures','显示|隐藏']]],
 sleepSettings:['锁屏与电源',[['锁屏样式','style','阅读票根|书架拼贴|壁纸锁屏'],['自动休眠','auto','1 分钟|5 分钟|10 分钟|关闭'],['关机睡眠','sleep','浅睡后深睡|彻底关机']]],
};
pref.aa??='开启';pref.fine??='0';
const routes={quick:'快捷面板',manager:'管理书架',library:'书目',storage:'存储卡',browser:'文件管理',fileOps:'文件操作',destination:'选择目标目录',confirm:'确认操作',keyboard:'输入',stats:'阅读记录',wireless:'无线网络',network:'网络详情',transferHub:'文件传输',host:'传输工作台',bluetooth:'蓝牙翻页器',learning:'按键学习',keys:'按键映射',mapping:'选择功能',profile:'个人资料',clock:'日期与时间',security:'锁屏密码',unlock:'输入密码',locked:'折角',updates:'系统升级',updateSteps:'升级进度',backup:'保存与恢复',shelfDirectory:'册序',fileShelfDirectory:'文件',gallery:'图片浏览',photo:'图像预览',cloud:'云同步',month:'近 30 天',installer:'电脑安装器',powerMenu:'电源选项',status:'设备提示',start:'首次使用',wallpaper:'壁纸',avatar:'头像',design:'界面索引',...Object.fromEntries(Object.entries(groups).map(([k,v])=>[k,v[0]]))};
const jobs={remove:['移出书架','所选书籍仍保留在 TF 卡，重新打开可再次上架。'],erase:['删除书籍','将从演示存储卡移除所选书籍及其书架条目。'],clear:['清除进度','所选书籍将从第一页开始。'],delFile:['删除文件','将移除当前选中的演示文件。'],format:['格式化 TF 卡','将清空演示卡上的书籍和目录。'],forget:['遗忘网络','移除该网络保存的连接信息。'],restore:['恢复备份','备份中的设置与阅读记录将替换当前演示配置。'],install:['安装升级','下载完成后仍须校验，再进入重启阶段。'],boot:['进入 BOOT 模式','演示设备将进入电脑刷机等待页。'],restart:['重新启动','演示重新进入启动页。'],off:['关机','保留最后一帧，唤醒后继续。']};
function ask(k){job=k;push('confirm')}
function beginEdit(k,value=''){edit=k;buf=value;caret=value.length;composition='';candPage=0;ime=k==='password'?'英文':'拼音';push('keyboard')}
const calendar=()=>'<div class="bars" aria-label="七日示例记录">'+[36,18,48,4,62,28,42].map(n=>'<i style="height:'+n+'px"></i>').join('')+'</div><div class="line mono"><small>五　六　日　一　二　三　四</small><small>近 7 天</small></div>';
function manage(){let list=shelfIds().filter(j=>sourceMatch(j)&&[currentName(j),['heanshouji hasj','xiangmoshiyi xmsy','yuzhonglaixin yzlx','shanchuangxiaolu scxl','yanhaimanche y h'][j]].join(' ').toLowerCase().includes(query.toLowerCase()));list.sort((a,b)=>Number(pref.fav.includes(b))-Number(pref.fav.includes(a))||(pref.sort==='按最近'?state.recent.indexOf(a)-state.recent.indexOf(b):currentName(a).localeCompare(currentName(b),'zh')));
return title('书目')+'<div class="line">'+button(esc(query||'搜索书名 / 拼音'),'edit:search','chip')+button(pref.sort,'sort','chip')+'</div>'+choice('source',['全部','TF 卡','内置','收藏','EPUB','TXT'])+
(list.map(j=>button('<span>'+(selected.includes(j)?'☑':'□')+'　'+esc(currentName(j))+'</span><small>'+(pref.fav.includes(j)?'收藏　':'')+books[j].type+'</small>','select:'+j)).join('')||hint('没有匹配的书。可清除搜索或调整来源。'))+
section('已选 '+selected.length+' 本','<div class="subnav">'+[['全选','all'],['收藏切换','fav'],['移出书架','ask:remove'],['清除进度','ask:clear'],['删除文件','ask:erase']].map(([s,a],k)=>k&&!selected.length?'<button disabled>'+s+'</button>':button(s,a,'chip')).join('')+'</div>')+button('重新扫描书架','rescan')+button('完成','back')}
const filesAt=()=>ids().filter(j=>(pref.fileFolder[j]||'/books')===dir);
const foldersAt=()=>dir==='/'?['books','fonts','pictures']:dir==='/books'?pref.folders:[];
function keyboard(){let chars=ime==='数字'?'1234567890':ime==='符号'?'，。！？、：；（）《》-_.@':ime==='九宫格'?['abc','def','ghi','jkl','mno','pqrs','tuv','wxyz']:Array.from(upper?'QWERTYUIOPASDFGHJKLZXCVBNM':'qwertyuiopasdfghjklzxcvbnm');
return title(inputs[edit]||'输入')+'<label class="meta">可用实体键盘直接编辑</label><input class="wide" id="field" autocomplete="off" type="'+(edit==='password'?'password':'text')+'" value="'+esc(buf)+'">'+
'<div class="subnav">'+['拼音','九宫格','英文','数字','符号'].map(t=>button(t,'ime:'+t,ime===t?'chosen chip':'chip')).join('')+'</div>'+
(composition?'<div class="notice">'+esc(composition)+'</div><div class="subnav">'+candidates().map(x=>button(x,'candidate:'+x,'chip')).join('')+button('›','candNext','chip')+'</div>':'')+
((ime==='英文'||ime==='拼音')?'<div class="qwerty">'+[chars.slice(0,10),chars.slice(10,19),chars.slice(19)].map(row=>'<div>'+row.map(t=>button(t,'char:'+t,'')).join('')+'</div>').join('')+'</div>':'<div class="'+(ime==='九宫格'||ime==='数字'?'numgrid':'keygrid')+'">'+Array.from(chars).map(t=>button(t,'char:'+t,'')).join('')+'</div>')+'<div class="subnav">'+[['◀','left'],['▶','right'],['空格','space'],['退格','del'],['重输','clear'],['a / A','case']].map(([s,a])=>button(s,'kbd:'+a,'chip')).join('')+'</div>'}
const lex={he:'河 和 合 荷 贺 何',an:'岸 安 案 暗',shu:'书 树 数 术',shui:'水 睡 税',du:'渡 读 度',kou:'口 扣',heanshouji:'河岸手记',dukou:'渡口',ni:'你 尼 泥',hao:'好 号 浩',nihao:'你好'};
function candidates(){let raw=composition.replaceAll(' ','');if(ime==='九宫格'){let keys=['abc','def','ghi','jkl','mno','pqrs','tuv','wxyz'];let matches=Object.keys(lex).filter(word=>Array.from(word).map(ch=>keys.findIndex(g=>g.includes(ch))+2).join('').startsWith(raw));let words=[...new Set(matches.flatMap(w=>lex[w].split(' ')))];return words.slice(candPage*4,candPage*4+4)}let words=(lex[raw]||Object.entries(lex).filter(([k])=>k.startsWith(raw)).map(x=>x[1]).join(' ')||raw).split(' ').filter(Boolean);return words.slice(candPage*4,candPage*4+4)}
let cloudState=params.get('cloud')||'unbound',cloudSet=new Set(),imported=new Set(),photoIndex=Number(params.get('photo')||0)%3,photos=['河纹','等高线','环形谱'],installerStage=0,step=0, formatDone=false,savedBackup=null, previewLock='',focusPage=0;const fileName=()=>/^b\d$/.test(file)?currentName(+file[1])+'.'+books[+file[1]].type.toLowerCase():file;
function details(v){
 if(groups[v]){let extra={appearance:link('个人资料','profile'),typography:section('辅助线位置','<div class="subnav">'+button('上移 2px','offset:-2','chip')+button('居中','offset:0','chip')+button('下移 2px','offset:2','chip')+'</div>')+link('字号与行距','type'),controls:link('实体按键映射','keys'),sleepSettings:link('壁纸选择','wallpaper')+link('四位密码','security')+button('预览当前锁屏','lock')};return title(routes[v])+groups[v][1].map(([s,k,val])=>section(s,choice(k,val.split('|')))).join('')+extra[v]}
 switch(v){
 case 'gallery':return title('图片')+hint('/pictures　／　3 张示例版画')+photos.map((name,i)=>button('<span class="print print'+i+'"></span><b>'+name+'</b><small>预览　›</small>','photo:'+i)).join('');
 case 'photo':return '<div class="photo print'+photoIndex+'"></div><h2>'+photos[photoIndex]+'</h2>'+hint('图片 '+(photoIndex+1)+' / 3')+'<div class="rail">'+button('上一张','image:-1','')+button('下一张','image:1','')+'</div>'+button('设为锁屏壁纸','photoWall','primary');
 case 'cloud':return title('云同步')+(cloudState==='unbound'?section('绑定设备',hint('连接 Wi-Fi 后，用阅星瞳 APP 扫描设备显示的绑定码。演示环境可直接模拟扫码结果。')+link('无线网络','wireless')+button('模拟完成扫码','cloudBind','primary')):cloudState==='failed'?section('校验失败',hint('未将失败文件写入卡中。已导入项保留。')+button('重试失败项','cloudRetry','primary')):cloudState==='unbind'?section('解除绑定？',hint('只移除本机连接信息。已导入的文件保留。')+button('保留绑定','cloudCancel')+button('确认解绑','cloudForget','primary')):section('云端清单 · 示例',['河岸附录.epub','河纹.png','书宋.ttf'].map((x,i)=>button('<b>'+(cloudSet.has(i)?'☑':'□')+'　'+x+'</b><small>'+(imported.has(i)?'已导入':'待下载')+'</small>','cloudPick:'+i)).join('')+button(cloudSet.size?'同步所选':'一键同步','cloudImport','primary')+button('刷新列表','cloudRefresh')+button('解除绑定','cloudUnbind')));
 case 'month':return title('近 30 天')+hint('示例阅读记录，墨色越深表示读得越久。')+'<div style="display:grid;grid-template-columns:repeat(7,1fr);gap:4px">'+Array.from({length:30},(_,i)=>'<b style="font:14px monospace;text-align:center;padding:8px;background:'+['#eee','#ccc','#aaa','#fff'][(i*7)%4]+'">'+(i+1)+'</b>').join('')+'</div>';case 'quick':return title('快捷面板')+[ ['无线网络',pref.wifi,'quickWifi'],['蓝牙翻页器',pref.bt,'quickBt'],['手动全刷','清理残影','refreshNow'],['锁屏',pref.style,'lock'] ].map(([s,val,a])=>button('<b>'+s+'</b><span>'+esc(val)+'</span>',a)).join('');
 case 'manager':return manage();
 case 'stats':return title(statsBook?currentName(state.id):'阅读记录')+(statsBook?section('本书 · 示例记录',hint('阅读进度　'+(page()+1)+' / '+b().pages.length+' 页　　本次 6 分钟')+button('查看书签','showMarks')+link('最近 30 天','month')):'')+section('近七日 · 示例记录',calendar())+section('最近阅读',state.recent.filter(j=>ids().includes(j)).map(j=>button('<b>'+esc(currentName(j))+'</b><small>第 '+(state.pos[j]+1)+' 页</small>','read:'+j)).join(''));
 case 'storage':return title('存储卡')+section('TF 卡　／　FAT32',hint('演示容量　7.4 GB 可用 / 8 GB'))+link('浏览卡内文件','browser')+button('重新检测','rescan')+button('格式化存储卡','ask:format');
 case 'shelfDirectory':return directoryPicker();
 case 'fileShelfDirectory':return title('书架目录')+'<div class="ticket"><small>来自文件管理</small><h2>'+esc(directoryName(pendingShelfDir))+'</h2><code>'+esc(pendingShelfDir)+'</code></div>'+hint('应用后，书架将显示此目录中的 '+shelfIds(pendingShelfDir).length+' 本藏书。');
 case 'browser':return title('文件')+'<div class="line"><span class="mono">'+esc(dir)+'</span>'+button('新建目录','edit:folder','chip')+'</div>'+(dir!=='/'?button('‹ 上级目录','up'):'')+foldersAt().map(f=>button('<b>'+esc(f)+'</b><span>目录　›</span>',(dir==='/books'?'dirOps:':'dir:')+f)).join('')+filesAt().map(j=>button('<b>'+esc(currentName(j))+'</b><span>'+books[j].type+'　›</span>','file:b'+j)).join('')+pref.added.filter(a=>a.dir===dir).map(a=>button(esc(a.name)+'　›','file:'+a.name)).join('')+(!foldersAt().length&&!filesAt().length&&!pref.added.some(a=>a.dir===dir)?hint('此目录为空。'):'')+(directoryPaths().includes(dir)?button('将此目录设为书架目录','fileScope:'+encodeURIComponent(dir),'rule'):'')+link('卡状态与容量','storage');
 case 'fileOps':if(file.startsWith('dir/'))return title('文件夹操作')+hint(esc(file.slice(4)))+button('进入文件夹','dirOpen')+button('设为书架目录','fileScope:'+encodeURIComponent(dir+'/'+file.slice(4)))+button('重命名目录','edit:rename');return title('文件操作')+section('选中项目',hint(esc(fileName())))+(/^b\d$/.test(file)?button('打开阅读','read:'+file[1]):pref.added.find(a=>a.dir===dir&&a.name===file)?.book!=null?button('打开阅读','read:'+pref.added.find(a=>a.dir===dir&&a.name===file).book):'')+button('重命名','edit:rename')+button('复制到…','dest:copy')+button('移动到…','dest:move')+button('删除文件','ask:delFile');
 case 'destination':return title(clip==='move'?'移动到':'复制到')+section('选择目标', ['/books',...pref.folders.map(x=>'/books/'+x),'/'].map(x=>button(esc(x)+(dest===x?'　✓':''),'target:'+x)).join(''))+button('确认目标目录','paste','primary');
 case 'confirm':{let j=jobs[job]||jobs.install;return title(j[0])+hint(esc((['remove','erase','clear'].includes(job)?selected.map(currentName).join('、'):job==='delFile'?fileName():'')))+'<div class="empty"><h2>请确认</h2>'+hint(j[1])+'</div>'+button('取消，返回','back')+button('确认'+j[0],'confirmed','primary')}
 case 'keyboard':return keyboard();
 case 'wireless':return title('无线网络')+section('当前连接',hint(esc(pref.wifi)))+section('附近网络',['阅览室','家庭网络','手机热点'].map(x=>button('<b>'+x+'</b><span>'+(pref.wifi===x?'已连接':pref.wifiSaved.includes(x)?'已保存':'需口令')+'　›</span>','network:'+x)).join(''))+button('重新扫描','scanWifi');
 case 'network':return title(esc(network))+hint('演示无线网络　／　2.4 GHz')+button(pref.wifi===network?'断开连接':pref.wifiSaved.includes(network)?'连接网络':'输入演示口令',pref.wifi===network?'disconnect':pref.wifiSaved.includes(network)?'connect':'edit:password')+(pref.wifiSaved.includes(network)?button('遗忘此网络','ask:forget'):'');
 case 'transferHub':return title('文件传输')+'<div class="subnav">'+['Wi-Fi','设备热点','USB'].map(x=>button(x,'netmode:'+x,netmode===x?'chosen chip':'chip')).join('')+'</div>'+section(netmode, hint(({ 'Wi-Fi':'电脑与阅读器连接同一网络后开启传输。','设备热点':'电脑先加入阅读器创建的热点，再进入传输工作台。','USB':'读卡期间电脑独占存储卡，结束前请先在电脑安全弹出。'})[netmode]))+button((server||usb?'结束':'开始')+(netmode==='USB'?'读卡':'传输'),'server','primary')+(server?link('打开传输工作台','host'):'')+hint('离线演示，连接状态仅在此原型内生效。');
 case 'host':return title('传输工作台')+section('目标目录',choice('uploadDir',['/books','/fonts','/pictures','/']))+button('添加示例文件到当前目录','upload')+link('文件管理与新建目录','browser')+link('头像与签名','profile')+link('预览与裁剪壁纸','wallpaper')+link('字体管理','appearance')+button('结束传输','stopServer');
 case 'bluetooth':return title('蓝牙翻页器')+section('连接状态',hint(esc(pref.bt)))+button('扫描附近设备','scanBt')+button('Pico Remote　／　模拟配对','pair')+link('学习翻页键','learning')+button('断开连接','unpair');
 case 'learning':return title('按键学习')+hint('选择要学习的功能，再按一下模拟翻页器。')+'<div class="subnav">'+['上一页','下一页'].map(x=>button(x,'learnKey:'+x,x===learnKey?'chosen chip':'chip')).join('')+'</div>'+button('模拟收到按键','learn','primary')+section('已记录',hint(pref.learn));
 case 'keys':return title('按键控制')+section('推荐方案',choice('preset',['推荐设置 1','推荐设置 2']))+['左键短按','中键短按','右键短按','左键长按','中键长按','右键长按'].map((k,j)=>button('<b>'+k+'</b><span>'+esc(pref[k]||['上一页','工具栏','下一页','全刷','首页','全屏'][j])+'　›</span>','map:'+k)).join('');
 case 'mapping':return title(keyslot)+['上一页','下一页','工具栏','首页','全刷','全屏','无操作'].map(x=>button(x,'bind:'+x)).join('')+hint('底部上滑始终可打开阅读工具栏。');
 case 'profile':return title('个人资料')+button('<b>读者姓名</b><span>'+esc(pref.name)+'　›</span>','edit:name')+button('<b>状态栏签名</b><span>'+esc(pref.sign||'未设置')+'　›</span>','edit:sign')+link('头像','avatar',esc(pref.avatar)+'　›');
 case 'avatar':return title('头像')+'<div class="ticket"><span class="stamp">'+esc(pref.name.slice(0,1)||'折')+'</span></div>'+choice('avatar',['字印','内置轮廓'])+hint('没有可读图片时，显示姓名首字。');
 case 'clock':return title('日期与时间')+button('<b>日期</b><span>'+esc(pref.date)+'</span>','edit:date')+button('<b>时间</b><span>'+esc(pref.time)+'</span>','edit:time')+button('网络对时','sync');
 case 'security':return title('四位密码')+hint(pref.pin?'已启用锁屏验证。':'唤醒后可用四位数字验证。请只使用演示数字。')+button(pref.pin?'更改密码':'设置演示密码','pin:start')+(pref.pin?button('关闭密码','pin:disable'):'');
 case 'unlock':return title(({old:'输入原密码',disable:'验证原密码',new:'设置四位密码',repeat:'再次输入',wake:'解锁'})[pinStage]||'解锁')+'<h2 class="mono">'+('● '.repeat(code.length)+'○ '.repeat(4-code.length))+'</h2><div class="numgrid">'+['1','2','3','4','5','6','7','8','9','清空','0','退格'].map(x=>button(x,'digit:'+x,'')).join('')+'</div>';
 case 'locked':{let style=previewLock||pref.style;return style==='书架拼贴'?'<div class="tiles">'+ids().map(j=>'<b>'+esc(currentName(j))+'</b>').join('')+'</div><h2>'+esc(pref.name)+'</h2>'+hint(ids().length+' 本藏书'):style==='壁纸锁屏'?'<div class="wall print'+Math.max(0,photos.indexOf(pref.wall))+'"></div><h2>'+esc(pref.wall)+'</h2>'+hint('壁纸锁屏'): '<i class="ribbon"></i><div class="ticket"><small>阅读票根</small><h1 class="serif">'+esc(currentName(state.id))+'</h1><p>'+b().chs[ch(state.id,page())]+'</p></div><div class="line"><small>读至第 '+(page()+1)+' 页</small><small>'+esc(pref.date)+'</small></div>'}
 case 'wallpaper':return title('壁纸预览')+'<div class="wall crop" style="background-size:'+((pref.zoom||1)*100)+'%;background-position:'+({靠左:'left',靠右:'right',居中:'center'}[pref.crop]||'center')+'"></div>'+section('裁剪位置',choice('crop',['居中','靠左','靠右'])+'<label>缩放　<input type="range" id="zoom" min="1" max="2.5" step="0.1" value="'+(pref.zoom||1)+'"></label>')+button('设为壁纸锁屏','useWall','primary');
 case 'updates':return title('系统升级')+hint('Pico　／　演示版本')+'<div class="subnav">'+['联网 OTA','TF 卡','历史版本'].map(x=>button(x,'updateMode:'+x,updateMode===x?'chosen chip':'chip')).join('')+'</div>'+button(updateMode==='联网 OTA'?'检查更新':'检查卡根目录','checkUpdate')+(checked?section('发现候选版本',hint(updateMode==='历史版本'?'目标为较早版本。可先备份当前设置。':'示例更新包已就绪。更新内容：阅读界面调整。')+button('核对并安装','ask:install','primary')):'')+button('电脑刷机 / BOOT','ask:boot');
 case 'updateSteps':{let arr=['正在下载','正在校验','正在安装','等待重启','演示更新完成'];return title(arr[step])+hint('目标　／　Pico 演示包')+'<div class="progress"><i style="width:'+((step+1)*20)+'%"></i></div>'+hint('第 '+(step+1)+' / 5 阶段　·　保持供电')+button(step===4?'返回系统升级':'演示下一阶段','nextStep','primary')}
 case 'backup':return title('保存与恢复')+hint('配置、个人资料、书签和进度随备份保存。书籍、字体、图片需单独复制。锁屏密码单独保留。')+section('TF 卡根目录',hint('Kiikoread-settings.backup'))+button('保存演示备份','saveBackup')+button('从演示备份恢复','ask:restore');
 case 'powerMenu':return title('电源选项')+button('锁屏','lock')+button('重新启动','ask:restart')+button('关机','ask:off');
 case 'installer':return title('电脑安装器')+section('目标设备',hint('RDP-G01-W　／　Pico'))+section('版本选择',choice('package',['当前正式版','历史版本']))+section('安装状态',hint(['准备数据线，选择对应设备。','串口已选择，等待确认安装。','正在写入演示镜像，保持连接。','校验通过，可重启设备。'][installerStage]))+button(['选择演示串口','确认演示安装','模拟校验','返回首次开机'][installerStage],'installNext','primary')+link('设备端 BOOT 入口','updates');case 'start':return '<i class="ribbon"></i>'+title('初次见面')+hint('把 EPUB 或 TXT 放进 TF 卡的 books 目录，然后扫描书架。')+link('检查存储卡','storage')+link('扫描并整理书架','manager')+link('开始使用','home');
 case 'status':{let x=statuses[statusKey]||statuses.recovery;return '<div class="empty"><small>设备提示</small><h2>'+x[0]+'</h2>'+hint(x[1])+'</div>'+button(x[3],'resolve:'+x[2],'primary')}
 default:return title('界面设计索引')+Object.entries(routes).filter(([k])=>k!=='design').map(([k,s])=>link(s,k)).join('');
 }}
const more=(arr)=>arr.map(([s,v])=>link(s,v)).join('');
function enhancedCore(){
 legacyDraw();let article=root.querySelector('article'),header=root.querySelector('header');header.lastElementChild.outerHTML=button(esc(pref.sign)+'　'+esc(pref.time)+'　▰ 82%','open:quick','');
 if(view==='home'){if(root.querySelector('.home h2'))root.querySelector('.home h2').textContent=currentName(state.id);article.insertAdjacentHTML('beforeend',button('阅读记录　›','stats:all','row'));if(!ids().length||pref.removed.includes(state.id))article.innerHTML=title('折角')+hint('从书架选择一本书，或先导入书籍。')+link('打开书架','shelf')}
 if(view==='shelf'){let list=shelfIds();list.sort((a,b)=>Number(pref.fav.includes(b))-Number(pref.fav.includes(a))||(pref.sort==='按最近'?state.recent.indexOf(a)-state.recent.indexOf(b):currentName(a).localeCompare(currentName(b),'zh')));let limit=pref.layout==='册序列表'?4:9,total=Math.max(1,Math.ceil(list.length/limit));focusPage=Math.max(0,Math.min(focusPage,total-1));article.innerHTML='<section class="shelf"><div class="line"><h1>书目</h1>'+button('管理','manager','chip')+'</div>'+shelfScope()+'<div class="line meta"><span>'+pref.sort+'</span><span>共 '+list.length+' 册</span></div>'+list.slice(focusPage*limit,(focusPage+1)*limit).map((j,k)=>button('<span class="number">0'+(focusPage*limit+k+1)+'</span><span><strong>'+esc(currentName(j))+'</strong><small>'+books[j].chs[ch(j,state.pos[j])]+'　／　'+books[j].type+'</small></span><span>'+(pref.fav.includes(j)?'★':'›')+'</span>','read:'+j,'book '+(k===0?'active':''))).join('')+(total>1?'<div class="pager">'+(focusPage?button('‹ 上一页','shelfPage:-1',''):'<span>首页</span>')+'<small>'+(focusPage+1)+' / '+total+'</small>'+(focusPage<total-1?button('下一页 ›','shelfPage:1',''):'<span>末页</span>')+'</div>':'')+(list.length?'':hint(pref.shelfDir?'这个目录还没有可显示的书。可以更换目录，或恢复默认书架。':'还没有藏书，进入管理书架重新扫描。'))+'</section>';let style=article.querySelector('.shelf');if(pref.layout!=='册序列表'){style.querySelectorAll('.book').forEach(x=>{x.style.cssText='width:48%;height:120px;display:inline-flex;vertical-align:top;justify-content:center;border:1px solid;margin:1%;background:'+(pref.layout==='深色书轨'?'#222;color:white':'#eee;color:#111');x.querySelector('.number').remove()})}}
 if(view==='files')article.innerHTML=title('文件')+section('TF 卡　／　FAT32',link('浏览目录','browser')+link('容量与卡状态','storage'))+section('媒体与传输',link('图片浏览','gallery')+link('文件传输','transferHub')+link('云同步','cloud'));
 if(view==='settings')article.innerHTML=title('设置')+section('阅读与显示',more([['阅读设置','readingSettings'],['显示与个性化','appearance']]))+section('连接与设备',more([['无线网络','wireless'],['蓝牙翻页器','bluetooth'],['文件传输','transferHub'],['个人资料','profile'],['日期与时间','clock'],['锁屏与电源','sleepSettings']]))+section('升级与恢复',more([['系统升级','updates'],['保存与恢复','backup'],['电源选项','powerMenu']]));
 if(view==='reader'){header.firstElementChild.textContent='‹　'+currentName(state.id);let sheet=root.querySelector('.sheet');if(sheet){sheet.lastElementChild.remove();sheet.insertAdjacentHTML('beforeend',link('阅读设置','readingSettings')+button('本书阅读统计','stats:book'))}
 let prose=root.querySelector('.prose');prose.style.cssText='margin-inline:'+({窄:0,标准:4,宽:16}[pref.margin])+'px;font-family:'+(pref.font==='系统黑体'?'sans-serif':'"Songti SC",serif');prose.querySelectorAll('p').forEach(p=>{p.style.textIndent=(parseInt(pref.indent)+(+pref.fine)*.1)+'em';p.style.marginBottom=({紧凑:'.3em',标准:'.65em',宽松:'1.2em'})[pref.paragraph];if(pref.line!=='关闭'){p.style.textDecoration='underline';p.style.textDecorationStyle=pref.line==='点线'?'dotted':'dashed';p.style.textUnderlineOffset=(5+(pref.lineOffset||0))+'px'}});if(pref.immersive==='开启'&&!panel){root.querySelector('header').hidden=true;root.querySelector('header').style.display='none';root.querySelector('footer').style.display='none'}}
}
draw=function(){if(core.includes(view))enhancedCore();else shell(routes[view]||'折角',details(view),['shelfDirectory','fileShelfDirectory'].includes(view)?button('取消','back','')+button(view==='shelfDirectory'?'使用此目录':'设为书架目录','applyScope','scope-apply'):view==='locked'?button('唤醒','wake',''):view==='keyboard'?backBtn()+button('完成输入','submit',''):backBtn());let art=root.querySelector('article');if(notice)art.insertAdjacentHTML('afterbegin','<div role="status" class="notice">'+esc(notice)+'</div>');root.style.fontFamily=pref.systemFont==='内置宋体'?'"Songti SC",serif':'';root.style.color=pref.contrast==='柔和'?'#444':['鲜明','高对比'].includes(pref.contrast)?'#000':'#191919';if(view!=='reader'&&pref.scale!=='100%')art.style.fontSize=parseInt(pref.scale)/100+'em';if(view==='locked'||(view==='unlock'&&pinStage==='wake'))root.querySelector('header').style.pointerEvents='none';};
function status(k){statusKey=k;push('status')}
function readDemo(j){if(!ids().includes(j))return status('missing');pref.hidden=pref.hidden.filter(n=>n!==j);store();book(j,core.includes(view)?view:'files')}
function applyJob(){let k=job;if(k==='install'){step=0;push('updateSteps');return}
 if(k==='restore'){if(!savedBackup)return status('backup-error');let pin=pref.pin;Object.assign(state,structuredClone(savedBackup.state));Object.assign(pref,structuredClone(savedBackup.pref),{pin});save()}
 if(['remove','erase','clear'].includes(k)){if(k==='remove')pref.hidden=[...new Set([...pref.hidden,...selected])];if(k==='erase')pref.removed=[...new Set([...pref.removed,...selected])];if(k==='clear')selected.forEach(j=>state.pos[j]=0);selected=[];save()}
 if(k==='delFile'){if(/^b\d$/.test(file))pref.removed.push(+file[1]);else pref.added=pref.added.filter(a=>!(a.dir===dir&&a.name===file));trail=trail.filter(v=>v!=='fileOps')}
 if(k==='format'){pref.removed=books.map((_,i)=>i);pref.added=[];pref.folders=[];pref.shelfDir='';dir='/'}
 if(k==='forget'){pref.wifiSaved=pref.wifiSaved.filter(n=>n!==network);if(pref.wifi===network)pref.wifi='未连接'}
 if(k==='boot'){pop();return msg('BOOT 模式演示：等待电脑安装器连接。')}
 if(k==='restart'){trail=[];return go('start')}if(k==='off'){lockFrom='home';return go('locked')}
 store();pop();msg('已完成：'+jobs[k][0])}
function insert(s){buf=buf.slice(0,caret)+s+buf.slice(caret);caret+=s.length;composition='';candPage=0;draw()}
function submit(){let val=buf.trim();if(['name','rename','folder','password'].includes(edit)&&!val)return msg('请输入内容，再点完成。');
 if(['rename','folder'].includes(edit)&&(/[\/\\]/.test(val)||['.','..'].includes(val)))return msg('名称不能包含路径分隔符。');
 if(edit==='folder'){if(dir!=='/books')return msg('此演示支持在 /books 内新建分类。');if(pref.folders.includes(val))return msg('该名称已存在。');pref.folders.push(val)}
 if(edit==='rename'&&file.startsWith('dir/')){let old=file.slice(4);if(pref.folders.includes(val))return msg('已有同名目录。');pref.folders=pref.folders.map(x=>x===old?val:x);for(let k in pref.fileFolder)if(pref.fileFolder[k]==='/books/'+old)pref.fileFolder[k]='/books/'+val;pref.added.forEach(x=>{if(x.dir==='/books/'+old)x.dir='/books/'+val});if(pref.shelfDir==='/books/'+old)pref.shelfDir='/books/'+val;file='dir/'+val}else if(edit==='rename'){if(ids().some(j=>currentName(j)===val&&file!=='b'+j)||pref.added.some(a=>a.dir===dir&&a.name===val))return msg('当前目录已有同名文件。');if(/^b\d$/.test(file))pref.aliases[+file[1]]=val;else{let a=pref.added.find(a=>a.dir===dir&&a.name===file);if(a)a.name=val;file=val}}
 if(edit==='password'){pref.wifi=network;if(!pref.wifiSaved.includes(network))pref.wifiSaved.push(network)}
 if(edit==='date'&&!/^\d{4}-\d{2}-\d{2}$/.test(val))return msg('日期格式为 YYYY-MM-DD。');
 if(edit==='time'&&!/^([01]\d|2[0-3]):[0-5]\d$/.test(val))return msg('请用 24 小时制输入 HH:MM。');
 if(['name','sign','time','date'].includes(edit))pref[edit]=val;
 if(edit==='search'){query=val;selected=[]}store();pop();msg(edit==='password'?'已连接演示网络':'已应用')}
function dispatch(action){let [a,x,y]=action.split(':');notice='';
 switch(a){
 case 'photo':photoIndex=+x;return push('photo');case 'image':photoIndex=(photoIndex+(+x)+3)%3;break;case 'photoWall':pref.wall=photos[photoIndex];pref.style='壁纸锁屏';store();return msg('已选为锁屏壁纸。');
 case 'cloudBind':if(pref.wifi==='未连接')return msg('请先打开无线网络并连接演示网络。');cloudState='list';break;case 'cloudPick':if(!imported.has(+x))cloudSet.has(+x)?cloudSet.delete(+x):cloudSet.add(+x);break;case 'cloudImport':for(let j of cloudSet.size?cloudSet:[0,1,2])imported.add(j);cloudSet.clear();return msg('所选内容已完成模拟导入。');case 'cloudRefresh':return msg('云端列表已刷新。');case 'cloudRetry':cloudState='list';break;case 'cloudUnbind':cloudState='unbind';break;case 'cloudCancel':cloudState='list';break;case 'cloudForget':cloudState='unbound';cloudSet.clear();break;
 case 'installNext':if(installerStage===3){installerStage=0;return push('start')}installerStage++;break;case 'back':if(view==='locked')return dispatch('wake');if(view==='unlock'&&pinStage==='wake')return go('locked');return pop();
 case 'open':if(view==='locked'||(view==='unlock'&&pinStage==='wake'))return;if(x==='type')returnTo=view;if(x==='browser'&&usb)return status('busy');return push(x);
 case 'pickDirectory':pendingShelfDir=pref.shelfDir;return push('shelfDirectory');
 case 'chooseScope':pendingShelfDir=decodeURIComponent(x);return draw();
 case 'fileScope':pendingShelfDir=decodeURIComponent(x);return push('fileShelfDirectory');
 case 'applyScope':return setShelfDirectory(pendingShelfDir);
 case 'resetScope':return setShelfDirectory('');
 case 'shelfPage':focusPage+=Number(x);break;case 'read':return readDemo(+x);case 'manager':selected=[];return push('manager');
 case 'offset':pref.lineOffset=x==='0'?0:Math.max(-10,Math.min(10,(pref.lineOffset||0)+Number(x)));break;case 'set':pref[x]=y;if(x==='source')selected=[];if(x==='preset')['左键短按','中键短按','右键短按','左键长按','中键长按','右键长按'].forEach((key,j)=>pref[key]=(y==='推荐设置 2'?['首页','全屏','工具栏','无操作','全刷','无操作']:['上一页','工具栏','下一页','无操作','全刷','无操作'])[j]);break;
 case 'sort':pref.sort=pref.sort==='按最近'?'按名称':'按最近';selected=[];break;
 case 'select':selected=selected.includes(+x)?selected.filter(j=>j!==+x):[...selected,+x];break;
 case 'all':selected=[...root.querySelectorAll('[data-act^="select:"]')].map(el=>+el.dataset.act.split(':')[1]);break;
 case 'fav':selected.forEach(j=>pref.fav=pref.fav.includes(j)?pref.fav.filter(n=>n!==j):[...pref.fav,j]);selected=[];break;
 case 'ask':return ask(x);case 'confirmed':return applyJob();case 'rescan':return msg('扫描完成，找到 '+ids().length+' 本书。');
 case 'showMarks':chapterTab='书签';return push('contents');case 'stats':statsBook=x==='book';return push('stats');
 case 'dirOps':file='dir/'+x;return push('fileOps');case 'dirOpen':dir=dir+'/'+file.slice(4);return push('browser');case 'dir':dir=(dir==='/'?'':dir)+'/'+x;break;case 'up':dir=dir.slice(0,dir.lastIndexOf('/'))||'/';break;
 case 'file':file=x;return push('fileOps');case 'dest':clip=x;dest=dir;return push('destination');case 'target':dest=x;break;
 case 'paste':{let name=fileName();if(dest===dir||pref.added.some(i=>i.dir===dest&&i.name===name))return status('collision');if(clip==='move'&&/^b\d$/.test(file))pref.fileFolder[+file[1]]=dest;else{if(clip==='move')pref.added=pref.added.filter(a=>!(a.dir===dir&&a.name===file));pref.added.push({dir:dest,name,book:/^b\d$/.test(file)?+file[1]:undefined})}store();pop();return msg('已'+(clip==='move'?'移动':'复制')+'到 '+dest)}
 case 'edit':return beginEdit(x,x==='search'?query:x==='rename'?(/^b\d$/.test(file)?currentName(+file[1]):file.replace(/^dir\//,'')):pref[x]||'');
 case 'ime':ime=x;composition='';break;case 'candNext':candPage++;if(!candidates().length)candPage=0;break;case 'candidate':return insert(x);
 case 'char':if(ime==='拼音'||ime==='九宫格'){composition+=x.length===1?x:String(['abc','def','ghi','jkl','mno','pqrs','tuv','wxyz'].indexOf(x)+2);candPage=0}else return insert(x);break;
 case 'kbd':if(x==='left')caret=Math.max(0,caret-1);if(x==='right')caret=Math.min(buf.length,caret+1);if(x==='space')return insert(' ');if(x==='clear'){if(composition)composition='';else{buf='';caret=0}}if(x==='case')upper=!upper;if(x==='del'){if(composition)composition=composition.slice(0,-1);else if(caret){buf=buf.slice(0,caret-1)+buf.slice(caret);caret--}}break;
 case 'submit':return submit();
 case 'network':network=x;return push('network');case 'scanWifi':return msg('扫描完成，发现 3 个网络。');
 case 'connect':pref.wifi=network;break;case 'disconnect':pref.wifi='未连接';break;
 case 'quickWifi':if(pref.wifi!=='未连接')pref.wifi='未连接';else if(pref.wifiSaved.length)pref.wifi=pref.wifiSaved[0];else return push('wireless');break;
 case 'quickBt':pref.bt=pref.bt==='关闭'?'待配对':'关闭';break;
 case 'refreshNow':return msg('本次全刷已完成（界面演示）。');
 case 'netmode':if(server||usb)return msg('先结束当前传输，再切换连接方式。');netmode=x;break;
 case 'server':if(netmode==='USB')usb=!usb;else if(netmode==='Wi-Fi'&&pref.wifi==='未连接')return push('wireless');else server=!server;break;
 case 'upload':{let path=pref.uploadDir||'/books',name='传入示例-'+(pref.added.length+1)+'.txt';pref.added.push({dir:path,name});store();return msg('已添加 '+path+'/'+name)}
 case 'stopServer':server=false;usb=false;return pop();
 case 'scanBt':return msg('发现 1 台设备：Pico Remote。');case 'pair':pref.bt='已连接';break;case 'unpair':pref.bt='已断开';break;
 case 'learnKey':learnKey=x;break;case 'learn':if(pref.bt!=='已连接')return msg('请先配对翻页器。');pref.learn=learnKey+' · 已记录';break;
 case 'map':keyslot=x;return push('mapping');case 'bind':pref[keyslot]=x;store();return pop();
 case 'sync':if(pref.wifi==='未连接')return msg('请先连接无线网络。');pref.time='08:42';pref.date='2026-11-26';notice='已按示例时钟校准。';break;
 case 'useWall':pref.style='壁纸锁屏';notice='已应用壁纸。';break;
 case 'lock':lockFrom=view==='quick'?(trail.at(-1)||'home'):view;previewLock='';return push('locked');
 case 'wake':if(pref.pin){pinStage='wake';code='';return push('unlock')}trail=[];return go(lockFrom);
 case 'pin':pinStage=pref.pin?(x==='disable'?'disable':'old'):'new';code='';return push('unlock');
 case 'digit':if(x==='清空')code='';else if(x==='退格')code=code.slice(0,-1);else code+=x;if(code.length===4){let val=code;code='';if(['wake','old','disable'].includes(pinStage)&&val!==pref.pin)return msg('密码不正确，请重新输入。');if(pinStage==='wake'){trail=[];return go(lockFrom)}if(pinStage==='disable'){pref.pin='';store();return pop()}if(pinStage==='old')pinStage='new';else if(pinStage==='new'){firstCode=val;pinStage='repeat'}else if(pinStage==='repeat'){if(val!==firstCode){pinStage='new';return msg('两次输入不同，请重新设置。')}pref.pin=val;store();return pop()}}break;
 case 'updateMode':updateMode=x;checked=false;break;case 'checkUpdate':checked=true;break;
 case 'nextStep':if(step===4)return go('updates');step++;break;
 case 'saveBackup':{let {pin,...safe}=pref;savedBackup=structuredClone({pref:safe,state});try{if(!demo)localStorage.setItem('folded-backup-v1',JSON.stringify(savedBackup))}catch{}return msg('已保存演示配置与阅读记录。')}
 case 'resolve':if(x==='unlock'){pinStage='wake';code=''}return push(x);
 }store();draw();if(view==='keyboard'){let el=document.querySelector('#field');el?.setSelectionRange(caret,caret)}}
root.addEventListener('click',e=>{let el=e.target.closest('[data-act]');if(el&&el.dataset.act==='kbd:del'&&repeated){repeated=false;return}if(el)dispatch(el.dataset.act)});
root.addEventListener('input',e=>{if(e.target.id==='zoom'){pref.zoom=Number(e.target.value);root.querySelector('.crop').style.backgroundSize=(pref.zoom*100)+'%';store()}if(e.target.id==='field'){buf=e.target.value;caret=e.target.selectionStart}});
root.addEventListener('keyup',e=>{if(e.target.id==='field'){caret=e.target.selectionStart;if(e.key==='Enter')submit()}});
root.addEventListener('pointerup',e=>{if(e.target.id==='field')caret=e.target.selectionStart});
try{if(!demo)savedBackup=JSON.parse(localStorage.getItem('folded-backup-v1'))}catch{}
if(params.has('status')){view='status';statusKey=params.get('status')}
if(params.has('job')){view='confirm';job=params.get('job');selected=[0];file='b0'}
if(params.has('edit')){edit=params.get('edit');view='keyboard'}
if(params.has('style')){view='locked';previewLock=params.get('style')}
if(view==='unlock')pinStage='new';
if(params.has('dir'))dir=params.get('dir');if(params.has('file'))file=params.get('file');if(params.has('ime'))ime=params.get('ime');if(params.has('tab'))chapterTab=params.get('tab');if(params.has('stage'))step=Math.max(0,Math.min(4,Number(params.get('stage'))||0));if(params.has('mode'))netmode=params.get('mode');if(params.has('pin'))pinStage=params.get('pin');if(params.has('query'))query=params.get('query');if(params.has('stats'))statsBook=true;if(params.has('clip'))clip=params.get('clip');if(params.has('update')){updateMode=params.get('update');checked=true}if(params.has('composition'))composition=params.get('composition');
let gesture=null,repeatTimer,repeatInterval,repeated=false;
root.addEventListener('pointerdown',e=>{if(e.target.closest('[data-act="kbd:del"]')){repeated=false;repeatTimer=setTimeout(()=>{repeated=true;dispatch('kbd:del');repeatInterval=setInterval(()=>dispatch('kbd:del'),125)},500)}});
for(let ev of ['pointerup','pointercancel','blur'])window.addEventListener(ev,()=>{clearTimeout(repeatTimer);clearInterval(repeatInterval)});

root.addEventListener('pointerdown',e=>{gesture={x:e.clientX,y:e.clientY,v:view}},{passive:true});
root.addEventListener('pointerup',e=>{if(!gesture||gesture.v!==view)return;let dx=e.clientX-gesture.x,dy=e.clientY-gesture.y,top=root.getBoundingClientRect().top,from=gesture;gesture=null;if(from.y-top<50&&dy>50&&view!=='locked'&&view!=='unlock')return push('quick');if(view==='shelf'&&Math.abs(dx)>55&&Math.abs(dx)>Math.abs(dy)){focusPage+=dx<0?1:-1;draw();return}if(view==='reader'&&!panel&&Math.abs(dx)>55&&Math.abs(dx)>Math.abs(dy)){let target=page()+(dx<0?1:-1);if(target>=0&&target<b().pages.length)pos(target)}});
root.addEventListener('click',e=>{if(view!=='reader'||panel||!e.target.closest('.readerpage'))return;let rect=root.getBoundingClientRect(),fr=pref.mode==='上下翻页'?(e.clientY-rect.top)/rect.height:(e.clientX-rect.left)/rect.width;if(pref.mode!=='仅滑动'&&(fr<.25||fr>.75)){e.stopImmediatePropagation();let target=page()+(fr<.25?-1:1);if(target>=0&&target<b().pages.length)pos(target)}},true);
document.addEventListener('keydown',e=>{if(view!=='reader'||e.target.matches('input'))return;let slots={ArrowLeft:'左键短按',Enter:'中键短按',ArrowRight:'右键短按'};if(!slots[e.key])return;e.preventDefault();let act=pref[slots[e.key]]||{ArrowLeft:'上一页',Enter:'工具栏',ArrowRight:'下一页'}[e.key];if(act==='工具栏'){panel=panel?'':'tools';draw()}if(act==='首页')go('home');if(act==='全屏'){pref.immersive=pref.immersive==='开启'?'关闭':'开启';draw()}if(act==='全刷')msg('已演示全刷。');if(act==='上一页'&&page()>0)pos(page()-1);if(act==='下一页'&&page()<b().pages.length-1)pos(page()+1)});
if(demo && params.has('scope') && directoryPaths().includes(params.get('scope'))) pref.shelfDir=params.get('scope');
pendingShelfDir = pref.shelfDir || '';
draw();
