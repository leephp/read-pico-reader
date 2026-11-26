// Pico screen pixels are displayed on a 380 CSS-pixel preview.
const readerScale = 380 / 684;
const readerPages = {pictureList:'卡内图片',picturePreview:'背景试读',readingSettings:'阅读设置',textStyle:'文字设置',pageLayout:'排版设置',pageDisplay:'阅读显示',readerGuide:'阅读线',chapterStyle:'章节题头'};
let readingTab = ['排版','翻页','手势'].includes(params.get('readingTab')) ? params.get('readingTab') : '排版';
let autoTurn = false, turnTimer;
const rsDefault = {
  px:Math.round(state.size / readerScale),weight:0,embedded:'关闭',
  marginPx:({窄:0,标准:12,宽:30})[pref.margin]??12,leadingPct:Math.max(110,Math.min(150,Math.round(state.leading*100))),
  paraPct:({紧凑:0,标准:25,宽松:75})[pref.paragraph]??25,trackPx:0,
  finePx:Number(pref.fine)||0,heading:'粗竖线',headingAlign:'左对齐',headingPct:100,
  vertical:'关闭',background:'纸白',seconds:10
};
function setting(key){return pref[key]??rsDefault[key]}
function readerButtons(label,key,values){
  return section(label,'<div class="rs-choices">'+values.map(v=>'<button data-act="set:'+key+':'+v+'" aria-pressed="'+(String(setting(key))===String(v))+'" class="'+(String(setting(key))===String(v)?'chosen':'')+'">'+esc(v)+'</button>').join('')+'</div>');
}
function readerRange(label,key,min,max,step,unit){
  return '<label class="rs-range"><span>'+label+'<output>'+setting(key)+' '+unit+'</output></span><input type="range" data-reading-range="'+key+'" data-unit="'+unit+'" min="'+min+'" max="'+max+'" step="'+step+'" value="'+setting(key)+'"></label>';
}
function applyPageStyle(container){
  const prose=container.querySelector('.prose'); if(!prose)return;
  const px=setting('px')*readerScale;
  Object.assign(prose.style,{fontSize:px+'px',lineHeight:setting('leadingPct')/100,letterSpacing:setting('trackPx')*readerScale+'px',marginInline:setting('marginPx')*readerScale+'px',fontFamily:pref.font==='系统黑体'?'sans-serif':'"Songti SC",serif',fontWeight:setting('weight')>12?'bold':'normal',webkitTextStroke:Math.max(0,setting('weight'))*.012+'px',webkitFontSmoothing:pref.aa==='关闭'?'none':'antialiased',writingMode:setting('vertical')==='开启'?'vertical-rl':'horizontal-tb'});
  if(setting('vertical')==='开启'){prose.style.height=container.classList.contains('readerpage')?'390px':'125px';prose.style.maxWidth='100%';prose.style.overflow='auto'}else prose.style.height='';
  container.classList.toggle('rs-bg',setting('background')==='细纹');
  prose.querySelectorAll('p').forEach(p=>Object.assign(p.style,{textIndent:((parseInt(pref.indent)||0)*px+setting('finePx')*readerScale)+'px',marginBottom:setting('paraPct')/100+'em',textDecorationLine:pref.line==='关闭'?'none':'underline',textDecorationStyle:pref.line==='点线'?'dotted':'dashed',textUnderlineOffset:(4+(pref.lineOffset||0)*readerScale)+'px'}));
  const h=container.querySelector('h2');if(h){h.classList.add('rs-heading');h.dataset.style=setting('heading');Object.assign(h.style,{fontSize:px*setting('headingPct')/100+'px',textAlign:({'左对齐':'left','居中':'center','右对齐':'right'})[setting('headingAlign')],fontFamily:prose.style.fontFamily})}
}
function readingSample(){return '<div class="rs-preview"><small>本书试排</small><h2>'+esc(b().chs[ch(state.id,page())])+'</h2><div class="prose">'+b().pages[page()].map(p=>'<p>'+esc(p)+'</p>').join('')+'</div></div>'}
function readingScreen(v){
  const tabs='<div class="rs-tabs">'+['排版','翻页','手势'].map(t=>'<button data-rstab="'+t+'" aria-current="'+(readingTab===t)+'">'+t+'</button>').join('')+'</div>';
  if(v==='pictureList')return pictureList();
  if(v==='picturePreview')return picturePreview();
  if(v==='readingSettings'){
    const layout=link('文字设置','textStyle',setting('px')+' px　›')+link('排版设置','pageLayout','边距与间距　›')+link('阅读显示','pageDisplay','全屏与竖排　›')+link('阅读线','readerGuide',pref.line+'　›')+link('章节题头','chapterStyle',setting('heading')+'　›');
    const turn=readerButtons('翻页效果','wave',['普通','水波纹','速读黑白'])+readerButtons('全刷间隔','snap',['5 页','10 页','15 页','30 页','不强制'])+button('<b>自动翻页</b><span>'+(autoTurn?'开启':'关闭')+'</span>','rsAuto')+readerRange('自动翻页间隔','seconds',1,10,1,'秒')+hint('回到正文后开始计时，打开工具时暂停。');
    const gesture=readerButtons('点击翻页区域','mode',['左右翻页','上下翻页','仅滑动'])+link('实体按键映射','keys','短按 / 长按　›')+readerButtons('电源键翻页','power',['关闭','开启'])+readerButtons('晃动翻页','shake',['关闭','开启'])+hint('阅读时从屏幕底部上滑可打开工具栏。');
    return '<div class="reading-menu">'+title('阅读设置')+tabs+(readingTab==='排版'?layout:readingTab==='翻页'?turn:gesture)+'</div>';
  }
  const fields={
   textStyle:()=>readerRange('正文字号','px',18,90,1,'px')+readerButtons('阅读字体','font',['内置宋体','系统黑体','卡内 TTF'])+readerRange('字重微调','weight',-32,32,1,'')+readerButtons('使用书内字体','embedded',['关闭','开启'])+readerButtons('文字抗锯齿','aa',['关闭','开启']),
   pageLayout:()=>readerRange('左右边距','marginPx',0,48,6,'px')+readerRange('行距','leadingPct',110,150,1,'%')+readerButtons('段距（%）','paraPct',[0,25,50,75])+readerButtons('字间距（px）','trackPx',[-4,-2,0,2,4])+readerButtons('首行缩进','indent',['0 字','1 字','2 字','3 字'])+readerRange('缩进微调','finePx',-20,20,1,'px'),
   pageDisplay:()=>readerButtons('竖排（右至左）','vertical',['关闭','开启'])+readerButtons('隐藏上下状态栏','immersive',['关闭','开启'])+readerButtons('书内图片','pictures',['显示','隐藏'])+link('阅读背景','pictureList',esc(pref.background==='卡内图片'?(pref.backgroundName||'卡内图片'):setting('background'))+'　›'),
   readerGuide:()=>readerButtons('线条样式','line',['关闭','虚线','点线'])+section('距默认位置　'+(pref.lineOffset||0)+' px','<div class="subnav">'+button('上移 2px','offset:-2','chip')+button('居中','offset:0','chip')+button('下移 2px','offset:2','chip')+'</div>'),
   chapterStyle:()=>readerButtons('题头样式','heading',['粗竖线','扉页横线','阶梯题头','章号侧栏'])+readerButtons('题头对齐','headingAlign',['左对齐','居中','右对齐'])+readerRange('题头字号','headingPct',80,200,10,'%')
  };
  return title(readerPages[v])+readingSample()+fields[v]();
}
const originalDraw=draw;
draw=function(){
  clearTimeout(turnTimer);
  const compat={type:'textStyle',typography:'pageLayout',controls:'readingSettings'};
  const pageView=compat[view]||view;
  if(view==='controls')readingTab='翻页';
  if(readerPages[pageView]){
    const scroll=root.querySelector('article')?.scrollTop||0;
    const same=root.dataset.readerPanel===view;
    shell(readerPages[pageView],readingScreen(pageView),button('‹ 返回','back','')+(pageView==='picturePreview'?button('使用此图','picApply','scope-apply'):pageView==='pictureList'?'':button('完成','rsDone','scope-apply')));
    root.dataset.readerPanel=view;
    const sample=root.querySelector('.rs-preview');if(sample){applyPageStyle(sample);cardPaint(sample,pageView==='picturePreview'?imagesOnCard[pictureIndex]?.url:pref.background==='卡内图片'?savedImageUrl:'')}
    cardPaint(root,'');
    if(same)root.querySelector('article').scrollTop=scroll;
    return;
  }
  delete root.dataset.readerPanel;originalDraw();
  cardPaint(root,view==='reader'&&pref.background==='卡内图片'?savedImageUrl:'');
  if(view==='reader'){applyPageStyle(root.querySelector('.readerpage'));if(autoTurn&&!panel){
    if(page()+1<b().pages.length)turnTimer=setTimeout(()=>pos(page()+1),setting('seconds')*1000);else autoTurn=false;
  }}
};
root.addEventListener('input',event=>{
  const el=event.target;if(!el.dataset.readingRange)return;
  pref[el.dataset.readingRange]=Number(el.value);el.closest('label').querySelector('output').textContent=el.value+' '+el.dataset.unit;
  const sample=root.querySelector('.rs-preview');if(sample)applyPageStyle(sample);store();
});
root.addEventListener('click',event=>{
  const tab=event.target.closest('[data-rstab]');if(tab){readingTab=tab.dataset.rstab;if(view==='controls')view='readingSettings';draw();return}
  const act=event.target.closest('[data-act]')?.dataset.act;
  if(act==='rsAuto'||act==='rsDone'){event.stopImmediatePropagation();if(act==='rsAuto'){autoTurn=!autoTurn;draw()}else{
    while(trail.length&&(readerPages[trail.at(-1)]||['controls','typography','type'].includes(trail.at(-1))))trail.pop();
    if(trail.length)pop();else go('reader');
  }}
},true);
draw();

loadBackground();
