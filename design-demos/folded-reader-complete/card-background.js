let imagesOnCard=[], pictureIndex=0, savedImageUrl='', photoNotice='', imageRevision=0, savingImage=false;
function cardPaint(node,url){
  if(!node)return;
  node.style.backgroundImage=url?'linear-gradient(#ffffffaa,#ffffffaa),url("'+url+'")':'';
  if(url){node.style.backgroundSize='cover';node.style.backgroundPosition='center'}
}
async function imageRecord(operation,blob){
  const db=await new Promise((resolve,reject)=>{
    const req=indexedDB.open('folded-reader-pictures',1);
    req.onupgradeneeded=()=>req.result.createObjectStore('background');
    req.onsuccess=()=>resolve(req.result);req.onerror=()=>reject(req.error);
  });
  try{return await new Promise((resolve,reject)=>{
    const tx=db.transaction('background',operation==='get'?'readonly':'readwrite');
    const table=tx.objectStore('background');
    const req=operation==='put'?table.put(blob,'current'):operation==='get'?table.get('current'):table.delete('current');
    tx.oncomplete=()=>resolve(req.result);tx.onabort=()=>reject(tx.error);
  })}finally{db.close()}
}
async function loadBackground(){
  if(demo||pref.background!=='卡内图片')return;
  const revision=imageRevision;
  try{const blob=await imageRecord('get');if(revision!==imageRevision)return;if(!blob)throw Error();savedImageUrl=URL.createObjectURL(blob);draw()}
  catch{photoNotice='上次保存的图片不可用，请重新选择。';draw()}
}
function pictureList(){
  return title('阅读背景')+section('当前背景',hint(esc(pref.background==='卡内图片'?pref.backgroundName:setting('background')))+
    '<div class="subnav">'+button('使用纸白','picPlain','chip')+button('使用细纹','picTexture','chip')+'</div>')+
    section('TF 卡　/　pictures','<div class="pic-input"><label>选择卡上的 pictures 文件夹<input data-pick="directory" type="file" webkitdirectory multiple></label><label>直接选择图片<input data-pick="file" type="file" accept=".jpg,.jpeg,.png" multiple></label></div>')+
    hint('从已挂载的 TF 卡选择 JPG 或 PNG，再点图片查看试读效果。')+
    (photoNotice?'<div class="notice" role="status">'+esc(photoNotice)+'</div>':'')+
    imagesOnCard.map((p,i)=>button('<img class="pic-thumb" alt="" src="'+p.url+'"><span class="pic-name">'+esc(p.name)+'</span>试读 ›','picSelect:'+i)).join('');
}
function picturePreview(){
  const pic=imagesOnCard[pictureIndex];if(!pic)return title('背景试读')+hint('请先选择图片。');
  return title('背景试读')+'<div class="line"><b class="pic-name">'+esc(pic.name)+'</b><small>'+(pictureIndex+1)+' / '+imagesOnCard.length+'</small></div>'+
    readingSample().replace('class="rs-preview"','class="rs-preview pic-proof"')+
    '<div class="subnav">'+(pictureIndex?button('‹ 上一张','picPrev','chip'):'')+(pictureIndex+1<imagesOnCard.length?button('下一张 ›','picNext','chip'):'')+'</div>'+hint(esc(photoNotice));
}
async function chooseBackground(mode){
  const item=mode==='卡内图片'?imagesOnCard[pictureIndex]:null;
  if(savingImage||(mode==='卡内图片'&&!item))return;savingImage=true;++imageRevision;
  try{
    if(!demo){if(item)await imageRecord('put',item.blob);else if(pref.background==='卡内图片')await imageRecord('delete')}
    if(savedImageUrl)URL.revokeObjectURL(savedImageUrl);
    savedImageUrl=item?URL.createObjectURL(item.blob):'';
    pref.background=mode;pref.backgroundName=item?.name||'';store();photoNotice='';
    while(['pictureList','picturePreview','pageDisplay'].includes(trail.at(-1)))trail.pop();go('pageDisplay');
  }catch{photoNotice='无法保存本地副本，请检查浏览器存储空间后重试。';draw()}finally{savingImage=false}
}
root.addEventListener('change',async event=>{
  const input=event.target;if(!input.dataset.pick||!input.files?.length)return;
  const revision=++imageRevision,items=[];let skipped=0;
  for(const file of input.files){
    const name=file.webkitRelativePath||file.name;
    if(name.split('/').some(part=>part.startsWith('.'))||!(/\.(png|jpe?g)$/i.test(file.name)))continue;
    if(file.size>50*1024*1024){skipped++;continue}
    const url=URL.createObjectURL(file);
    try{const img=new Image();img.src=url;await img.decode();items.push({url,name,blob:file})}catch{URL.revokeObjectURL(url);skipped++}
  }
  if(revision!==imageRevision){items.forEach(i=>URL.revokeObjectURL(i.url));return}
  imagesOnCard.forEach(i=>URL.revokeObjectURL(i.url));imagesOnCard=items.sort((a,b)=>a.name.localeCompare(b.name,'zh'));pictureIndex=0;
  photoNotice=(items.length?'已读取 '+items.length+' 张图片。':'未找到可用的 JPG 或 PNG。')+(skipped?'已跳过 '+skipped+' 张过大或损坏的图片。':'');
  if(input.dataset.pick==='file'&&items.length===1&&view==='pictureList')push('picturePreview');else draw();
});
root.addEventListener('click',event=>{
  const a=event.target.closest('[data-act]')?.dataset.act||'';
  if(!/^pic(Select:|Prev$|Next$|Apply$|Plain$|Texture$)/.test(a))return;
  event.stopImmediatePropagation();if(savingImage)return;
  if(a.startsWith('picSelect:')){pictureIndex=+a.slice(10);photoNotice='';push('picturePreview')}
  else if(a==='picPrev'||a==='picNext'){pictureIndex+=a==='picPrev'?-1:1;draw()}
  else chooseBackground(a==='picApply'?'卡内图片':a==='picPlain'?'纸白':'细纹');
},true);
