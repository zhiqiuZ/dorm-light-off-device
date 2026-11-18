// index_page.h — 内嵌页面（PROGMEM）v2.0.0 双通道版
// 实现 WEB_DESIGN.md + 改订附录：S1 全关主按钮 + 左右灯单控行（各含 关/长按开）
// S2 校准区分左/右两组角度 + 全局速度/保持；S4 帮助；D-3 长按/D-10 相对时间/D-11 N/4 保留。
// 全部资源内嵌，零外网依赖。
#pragma once
#include <pgmspace.h>

// ------------------------------------------------------------
// 主页面
// ------------------------------------------------------------
static const char PAGE_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang=zh-CN><head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name=theme-color content="#10141c">
<meta name=apple-mobile-web-app-capable content=yes>
<meta name=mobile-web-app-capable content=yes>
<title>宿舍关灯器</title>
<style>
:root{--bg:#10141c;--card:#182031;--fg:#eaf0ff;--mut:#93a1bd;--line:#25304a;--red:#e8452e;--grn:#37d67a;--amb:#ffb64a}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0}
body{background:radial-gradient(120% 80% at 50% 0,#1a2233,var(--bg) 60%) fixed,var(--bg);color:var(--fg);
 font:16px/1.5 system-ui,-apple-system,"PingFang SC","Microsoft YaHei",sans-serif;
 padding:calc(8px + env(safe-area-inset-top)) 16px calc(16px + env(safe-area-inset-bottom));
 max-width:520px;margin:auto;touch-action:manipulation;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
h1{font-size:17px;margin:0;font-weight:700}
#chip{font-size:12px;color:var(--mut);border:1px solid var(--line);border-radius:999px;padding:4px 10px}
#chip.on{color:var(--grn);border-color:#1e5c3a}#chip.busy{color:var(--amb);border-color:#6b4c18}
#chip.off{color:var(--mut)}#chip.re{color:#ff9d68;border-color:#6b3418}
#helpbtn{color:var(--mut);text-decoration:none;font-size:15px;padding:4px 10px;border:1px solid var(--line);border-radius:999px}
#banner{display:none;background:#3a2a10;color:var(--amb);border:1px solid #6b4c18;border-radius:12px;padding:8px 12px;font-size:13px;margin-bottom:10px}
#offBtn{width:100%;height:min(38vh,340px);border:0;border-radius:26px;color:#fff;cursor:pointer;
 background:linear-gradient(180deg,#ff7d63,#e8452e);box-shadow:0 10px 0 #a52d1c,0 18px 40px rgba(232,69,46,.3);
 font-size:clamp(28px,10vw,50px);font-weight:800;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px}
#offBtn small{font-size:13px;font-weight:600;opacity:.85}
#offBtn:active{transform:translateY(6px);box-shadow:0 4px 0 #a52d1c}
.lrow{display:flex;gap:10px;margin-top:12px}
.lcard{flex:1;background:var(--card);border:1px solid var(--line);border-radius:16px;padding:10px 10px 12px}
.lcard h2{font-size:14px;color:var(--mut);margin:0 0 8px;text-align:center;font-weight:700}
.loff{width:100%;height:52px;border:0;border-radius:12px;background:#2c1712;border:1px solid #55251c;
 color:#ff9d88;font:inherit;font-size:17px;font-weight:800;cursor:pointer}
.loff:active{background:#3d201a}
.onWrap{position:relative;margin-top:8px;border-radius:12px;overflow:hidden;border:1px solid #1e5c3a;background:#12241a}
.onFill{position:absolute;inset:0;width:0;background:#1c5e38}
.onb{position:relative;width:100%;height:48px;border:0;background:transparent;color:var(--grn);
 font:inherit;font-size:16px;font-weight:800;cursor:pointer;display:flex;align-items:center;justify-content:center;gap:6px}
.onb small{font-size:11px;font-weight:500;color:var(--mut)}
details{margin-top:14px;background:var(--card);border:1px solid var(--line);border-radius:14px}
summary{padding:12px 14px;font-weight:700;cursor:pointer;list-style:none}
summary::before{content:"▸ ";color:var(--mut)}details[open] summary::before{content:"▾ "}
.sec{padding:0 14px 14px;border-top:1px solid var(--line)}
.warn{font-size:12px;color:var(--amb);padding:10px 0 4px}
.grp{font-size:14px;font-weight:800;color:var(--fg);padding:12px 0 2px;border-bottom:1px dashed var(--line)}
.f{display:flex;align-items:center;gap:8px;font-size:13px;padding:6px 0;flex-wrap:wrap}
.f label{flex:1;min-width:80px;color:var(--mut)}
.f input[type=range]{flex:2;accent-color:var(--red)}
.f .v{width:52px;text-align:right;font-variant-numeric:tabular-nums}
.f input[type=checkbox]{width:22px;height:22px;accent-color:var(--grn)}
.seg{display:flex;gap:4px}.seg button{padding:8px 14px;border:1px solid var(--line);background:var(--bg);color:var(--mut);border-radius:8px;font:inherit;font-size:13px}
.seg button.sel{border-color:var(--red);color:#fff;background:#3a2019}
.ops{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}
.ops button{flex:1;min-width:80px;padding:10px 6px;border:0;border-radius:10px;background:#25304a;color:var(--fg);font:inherit;font-size:13px;cursor:pointer}
.ops .save{background:var(--grn);color:#04140b;font-weight:700}
footer{margin-top:14px;font-size:12px;color:var(--mut);display:flex;justify-content:space-between;gap:8px;flex-wrap:wrap}
footer a{color:var(--mut)}
#toast{position:fixed;left:50%;bottom:calc(24px + env(safe-area-inset-bottom));transform:translateX(-50%) translateY(20px);
 background:#0b0e14ee;border:1px solid var(--line);color:var(--fg);padding:10px 16px;border-radius:12px;
 font-size:14px;opacity:0;pointer-events:none;transition:.2s;max-width:88vw;text-align:center}
#toast.show{opacity:1;transform:translateX(-50%)}
#toast.err{border-color:#7c2a1d;color:#ffb6a8}
#help{font-size:13px;color:var(--mut);line-height:1.7}#help b{color:var(--fg)}#help code{color:var(--grn)}
</style></head><body>
<header><h1>宿舍关灯器</h1><div style="display:flex;gap:8px;align-items:center">
<span id=chip>连接中…</span><a id=helpbtn href=#help onclick="showHelp()">?</a></div></header>
<div id=banner>⚠ 新固件验证中，<span id=graceN>45</span> 秒内暂停动作。若反复重启，请到 <a href=/ota style="color:inherit">固件更新</a> 重传上一版 .bin。</div>
<button id=offBtn>全关<small>左右两盏一起关 · 依次执行约 4 秒</small></button>
<div class=lrow>
 <div class=lcard><h2>左灯</h2><button class=loff id=offL>关</button>
  <div class=onWrap><div class=onFill id=onLFill></div><button class=onb id=onLBtn>开<small>按住 0.6 秒</small></button></div></div>
 <div class=lcard><h2>右灯</h2><button class=loff id=offR>关</button>
  <div class=onWrap><div class=onFill id=onRFill></div><button class=onb id=onRBtn>开<small>按住 0.6 秒</small></button></div></div>
</div>
<details id=cfg><summary>设置 / 角度校准</summary><div class=sec>
 <div class=warn>⚠ 设错角度可能让舵机顶死；每个动作有分段超时强制断电。左右灯<b>各自校准</b>：改完先「试」，满意再「保存」。</div>
 <div class=grp>左灯（舵机 D2）</div>
 <div class=f><label>关灯角 A</label><input type=range id=offL_ min=10 max=170 step=1><span class=v id=vOffL></span></div>
 <div class=f><label>开灯角 B</label><input type=range id=onL_ min=10 max=170 step=1><span class=v id=vOnL></span></div>
 <div class=f><label>中位角</label><input type=range id=homeL_ min=10 max=170 step=1><span class=v id=vHomeL></span></div>
 <div class=f><label>方向镜像（装反了勾这个）</label><input type=checkbox id=mirL></div>
 <div class=ops><button data-trial=off-l>试·关左</button><button data-trial=on-l>试·开左</button></div>
 <div class=grp>右灯（舵机 D5）</div>
 <div class=f><label>关灯角 A</label><input type=range id=offR_ min=10 max=170 step=1><span class=v id=vOffR></span></div>
 <div class=f><label>开灯角 B</label><input type=range id=onR_ min=10 max=170 step=1><span class=v id=vOnR></span></div>
 <div class=f><label>中位角</label><input type=range id=homeR_ min=10 max=170 step=1><span class=v id=vHomeR></span></div>
 <div class=f><label>方向镜像（装反了勾这个）</label><input type=checkbox id=mirR></div>
 <div class=ops><button data-trial=off-r>试·关右</button><button data-trial=on-r>试·开右</button></div>
 <div class=grp>全局（两通道共用）</div>
 <div class=f><label>转角速度（慢=更稳）</label><div class=seg id=segSpeed>
   <button data-v=1>1最慢</button><button data-v=2>2</button><button data-v=3>3</button><button data-v=4>4最快</button></div></div>
 <div class=f><label>压住保持 ms</label><input type=range id=holdMs min=150 max=800 step=50><span class=v id=vHold></span></div>
 <div class=f><label>动作后回中位并松劲</label><input type=checkbox id=returnHome></div>
 <div class=ops><button class=save id=save>保存</button><button id=fact>恢复出厂默认</button></div>
 <div class=f style="margin-top:8px"><span id=eff></span></div>
</div></details>
<details id=vu><summary>语音 / 夜间模式</summary><div class=sec>
 <style>#vt{width:100%;border-collapse:collapse}#vt td{padding:4px 2px;font-size:14px}#vt .hex{color:#8a93a6;font-size:12px}#vu input[type=time]{background:#25304a;color:#eaf0ff;border:0;border-radius:8px;padding:6px}#vframes{font-size:12px;color:#8a93a6;word-break:break-all}</style>
 <div class=grp>语音（SU-03T）</div>
 <div class=f><label>启用语音</label><input type=checkbox id=vEn></div>
 <div class=f><span id=vBound style="color:#8a93a6">0/6 已绑定</span></div>
 <div class=warn>ℹ 学习绑定，不用抄协议：点「学」→ 15 秒内对模块喊唤醒词 + 说出那条命令 → 装置自动记住那串字节。</div>
 <table id=vt></table>
 <div class=grp>最近收到的帧（调试）</div>
 <div id=vframes>—</div>
 <div class=grp>夜间模式（省电）</div>
 <div class=f><label>窗口外深睡省电</label><input type=checkbox id=pwEn></div>
 <div class=f><label>醒着时段</label><input type=time id=pwS value="19:30"> → <input type=time id=pwE value="02:30"></div>
 <div class=ops><button id=pwApply>应用</button></div>
 <div class=f><span id=pwState style="color:#8a93a6;font-size:12px">—</span></div>
</div></details>
<details id=hp><summary>帮助 / 常见问题</summary><div class=sec id=help>
 <b>1. 怎么连上</b>：手机 WiFi 连 <code>GUANGDENG</code>，密码 <code>111222345</code>，
 浏览器开 <code>http://192.168.4.1</code>。若提示"无互联网"选<b>继续/保持</b>——正常现象。
 连上后浏览器打开<b>任意网页</b>都会自动进入本面板；注意连着本热点期间手机<b>无法上外网</b>，属正常。
 最多同时连 <b>4 台</b>，连不上先断开自己多余的旧设备。<br>
 <b>2. 加到桌面</b>：安卓 Chrome 菜单→"添加到主屏幕"；Safari 分享→"添加到主屏幕"。<br>
 <b>3. 点了没反应</b>：① 看 WiFi 是否又连回校园网或被自动切走（先连回 GUANGDENG）；② 关掉页面重开；
 ③ 页面拉到底点"诊断"，看装置是否重启/内存异常；④ 还不行 → 拔电再开。<br>
 <b>4. 全关为什么慢一点</b>：两个舵机<b>轮流干活</b>（先左后右），防止同时启动把电压拉垮——故意的。<br>
 <b>5. 哪边是左哪边是右</b>：以<b>面对开关面板</b>为准。装完先用设置页"试·关左/关右"认路，装反了勾"方向镜像"。<br>
 <b>6. 舵机动作变慢/推不动</b>：给电池充电（常挂链自动充），或在设置里调慢速度档、加过行程。<br>
 <b>7. 白天连不上热点</b>：正常——"夜间模式"下装置只在醒着时段（默认 19:30~02:30）开热点和语音，其余时间深睡省电（续航 3~5 天）。随时想用时进"语音 / 夜间模式"把它关掉。<b>手动按开关任何时候都不受影响</b>。<br>
 <b>8. 语音没反应</b>：① 语音开关是否被关；② 是否喊了唤醒词后 7 秒内说的命令；③ 装置是否处于窗口外（白天在睡觉）；④ "语音 / 夜间模式"→最近帧一片空白=模块没送话（查 D7/D6 线与 9600 波特率设置）。
</div></details>
<footer>
 <span id=facts>—</span>
 <span><a href=/diag target=_blank>诊断</a> · <a href=/ota target=_blank>固件更新</a> · <span id=ver></span></span>
</footer>
<div id=toast></div>
<script>
"use strict";
const $=s=>document.querySelector(s);
let lastUptime=-1,actTimer=0,toastT=0;
function toast(m,err){const t=$("#toast");t.textContent=m;t.className=err?"err show":"show";clearTimeout(toastT);toastT=setTimeout(()=>t.className="",2600);}
function chip(cls,txt){const c=$("#chip");c.className=cls;c.textContent=txt;}
async function api(u,opt,ms=3000){try{const c=new AbortController();const id=setTimeout(()=>c.abort(),ms);
 const r=await fetch(u,Object.assign({signal:c.signal},opt||{}));clearTimeout(id);
 let j=null;try{j=await r.json()}catch(e){}return{code:r.status,j};}catch(e){return{code:0,j:null};}}
function fmtAge(sec){if(sec<0)return"尚无动作";
 const d=new Date(Date.now()-sec*1000);const hm=d.getHours().toString().padStart(2,"0")+":"+d.getMinutes().toString().padStart(2,"0");
 if(sec<60)return"刚刚";if(sec<3600)return Math.floor(sec/60)+" 分钟前";return hm;}
function vib(n){if(navigator.vibrate)try{navigator.vibrate(n)}catch(e){}}
const LBL={"off-all":"全关","on-all":"全开","off-L":"关左灯","on-L":"开左灯","off-R":"关右灯","on-R":"开右灯"};
function lbl(a){return LBL[a]||a;}
async function refresh(first){const{code,j}=await api("/status",null,2500);
 if(code!=200){chip("off","无响应");busyNow=false;
  if(first&&first<4)setTimeout(()=>refresh(first+1),1500);   // 只在首屏重试，静止零轮询（WAC-7）
  return;}
 if(j.grace){$("#banner").style.display="block";$("#graceN").textContent=j.graceLeftSec;}
 else $("#banner").style.display="none";
 if(lastUptime>=0&&j.uptimeSec<lastUptime){chip("re","已重启");setTimeout(()=>chip("on","在线"),6000);}
 else chip(j.busy?"busy":"on",j.busy?(j.state==="ota"?"升级中":"动作中"):"在线");
 lastUptime=j.uptimeSec;busyNow=!!j.busy;
 $("#facts").textContent="本次开机已执行 "+j.countThisBoot+" 次 · "+(j.lastAction==="-"?"尚无动作":fmtAge(j.lastActionAgeSec)+" "+lbl(j.lastAction));
 $("#ver").textContent="v"+j.fwVer;
 if(!inited){inited=true;loadParams();}
 if(!tsDone){tsDone=true;var lo=Math.floor(Date.now()/1000-new Date().getTimezoneOffset()*60);api("/time?ts="+lo,{method:"POST"});}   // D10 v3.0.1：手机即时钟=本地墙钟秒（修 v3.0.0 时区差8h）
 paintSys(j);
 if(pending){pollAct();}}
let inited=false,pending=false,busyNow=false;
function pollAct(){clearTimeout(actTimer);
 actTimer=setTimeout(async()=>{const{code,j}=await api("/status",null,2500);
  if(code==200&&j.busy&&j.state!=="idle"){pollAct();}
  else{pending=false;refresh();          // 动作结束→刷新胶囊，不留"假动作中"
       if(code==200&&j.lastResult===2)toast("动作被中断（可能卡住了）",1);}},450);}
// side=off|on  ch=all|l|r  trial=1 不记数不去重
async function press(side,ch,trial){
 if(side==="off"&&!trial&&navigator.vibrate)vib(40);
 const{code,j}=await api("/press?side="+side+"&ch="+(ch||"all")+(trial?"&trial=1":""),{method:"POST"});
 if(code==0){chip("off","无响应");toast("装置没响应 —— 检查手机是否连着 GUANGDENG 热点（见 ? 帮助）",1);return;}
 if(code==409){toast(j&&j.errName==="ota"?"装置正在升级固件，暂不能操作":(busyNow?"装置正在执行上一个动作":"上一个操作还在进行，稍等一下"),1);return;}
 if(j&&j.deduped){toast("刚已经"+lbl((side==="off"?"off-":"on-")+(ch==="all"?"all":ch.toUpperCase().slice(0)))+"过一次啦");return;}
 if(j&&j.warn==="not_calibrated")toast("有灯还没校准角度，先点下方「设置」",1);
 if(trial){toast("已发送测试动作");return;}
 const nm=ch==="all"?(side==="off"?"全关指令已发出（先左后右）":"全开指令已发出"):
   "已"+(side==="off"?"关":"开")+(ch==="l"?"左灯":"右灯");
 toast(nm);pending=true;pollAct();
 refresh();}
// --- 长按开灯 0.6s（D-3），左/右各一套 ---
function makeLP(btnSel,fillSel,ch){let t=0,t0=0;const fill=$(fillSel);
 function down(e){e.preventDefault();if(busyNow){toast("装置正在执行上一个动作，等它结束");return;}
  t0=performance.now();fill.style.width="0";vib(15);
  t=setInterval(()=>{const p=Math.min(1,(performance.now()-t0)/600);fill.style.width=(p*100)+"%";
   if(p>=1){clearInterval(t);t=0;fill.style.width="0";press("on",ch);}},16);}
 function up(){if(t){clearInterval(t);t=0;fill.style.width="0";
  if(performance.now()-t0<550)toast("开灯需按住 0.6 秒");}}
 $(btnSel).addEventListener("pointerdown",down);
 ["pointerup","pointercancel","pointerleave"].forEach(ev=>$(btnSel).addEventListener(ev,up));}
makeLP("#onLBtn","#onLFill","l");
makeLP("#onRBtn","#onRFill","r");
$("#offBtn").onclick=()=>press("off","all");
$("#offL").onclick=()=>press("off","l");
$("#offR").onclick=()=>press("off","r");
// --- 校准区（S2）---
const KEYS=["offL_","onL_","homeL_","mirL","offR_","onR_","homeR_","mirR","speedMode","holdMs","returnHome"];
const PKEY={offL_:"angleOffL",onL_:"angleOnL",homeL_:"angleHomeL",mirL:"mirrorL",
 offR_:"angleOffR",onR_:"angleOnR",homeR_:"angleHomeR",mirR:"mirrorR"};
let cur={};
function val(k){const el=$("#"+k);return (k==="mirL"||k==="mirR"||k==="returnHome")?(el.checked?1:0):+el.value;}
function paint(){for(const k of KEYS){const v=cur[k];if(v===undefined)continue;
  const el=$("#"+k);if(k==="mirL"||k==="mirR"||k==="returnHome")el.checked=!!v;else if(k!=="speedMode")el.value=v;}
 for(const[k,vid]of[["offL_","vOffL"],["onL_","vOnL"],["homeL_","vHomeL"],["offR_","vOffR"],["onR_","vOnR"],["homeR_","vHomeR"]])
  $("#"+vid).textContent=cur[k]+"°";
 $("#vHold").textContent=cur.holdMs+"ms";
 document.querySelectorAll("#segSpeed button").forEach(b=>b.classList.toggle("sel",+b.dataset.v==cur.speedMode));
 const eL=cur.effL,eR=cur.effR;
 if(eL&&eR)$("#eff").textContent="生效角：左 关"+eL.off+"/开"+eL.on+"/中"+eL.home+"° · 右 关"+eR.off+"/开"+eR.on+"/中"+eR.home+"°";}
function readUI(){const o={};for(const k of KEYS)o[PKEY[k]||k]=(k==="speedMode")?(cur.speedMode||2):val(k);return o;}
["offL_","onL_","homeL_","offR_","onR_","homeR_","holdMs"].forEach(k=>$("#"+k).addEventListener("input",()=>{cur[k]=+$("#"+k).value;paint();}));
["mirL","mirR","returnHome"].forEach(k=>$("#"+k).addEventListener("change",()=>{cur[k]=val(k);paint();}));
document.querySelectorAll("#segSpeed button").forEach(b=>b.onclick=()=>{cur.speedMode=+b.dataset.v;paint();});
async function loadParams(){const{code,j}=await api("/params");if(code!=200||!j)return;
 cur={};for(const k of KEYS)cur[PKEY[k]||k]=j[PKEY[k]||k];cur.effL=j.effL;cur.effR=j.effR;paint();}
function qs(p){return Object.keys(p).filter(k=>k!=="effL"&&k!=="effR").map(k=>k+"="+p[k]).join("&");}
document.querySelectorAll("[data-trial]").forEach(b=>b.onclick=async()=>{
 const[side,ch]=b.dataset.trial.split("-");
 const{code,j}=await api("/setParams?"+qs(readUI())+"&dryRun=1",{method:"POST"});
 if(code==400){toast(errMsg(j),1);return;}if(code!=200){toast("装置无响应",1);return;}
 await press(side,ch,true);});
function errMsg(j){const e=j&&j.err;return e===3?"某个灯的两角度太接近或不同侧，舵机会顶不住":e===1?"数值超出允许范围":"参数被拒绝";}
$("#save").onclick=async()=>{const p=readUI();
 const{code,j}=await api("/setParams?"+qs(p),{method:"POST"});
 if(code==409){toast("装置忙，稍后再保存",1);return;}
 if(code==400){toast(errMsg(j),1);return;}
 if(code==500){toast("保存失败，别重复点，叫同学帮忙看下",1);return;}
 const same=j&&Object.keys(p).every(k=>+j[k]===+p[k]);
 if(same){toast("参数已保存，掉电不丢");cur=p;loadParams();}
 else{toast("保存失败：回读不一致，叫同学看下",1);loadParams();}};
$("#fact").onclick=async()=>{if(!confirm("恢复出厂默认会清掉左右灯校准角度，确定？"))return;
 const{code}=await api("/factoryReset",{method:"POST"});
 if(code==200){toast("已恢复默认，请重新校准左右灯");loadParams();}else toast("失败：装置忙或无响应",1);};
function showHelp(){const h=$("#hp");h.open=true;h.scrollIntoView({behavior:"smooth"});}
// --- v3 语音 / 夜间模式 ---
let tsDone=false;
function paintSys(j){
 if(j.voice){$("#vEn").checked=!!j.voice.en;$("#vBound").textContent=j.voice.bound+"/6 已绑定"+(j.voice.hit!=="-"?" · 上次语音:"+lbl(j.voice.hit):"");}
 if(j.pw){$("#pwEn").checked=!!j.pw.en;
  $("#pwState").textContent=(j.pw.clock?"时钟已同步":"时钟未同步")+" · 醒着 "+j.pw.win+(j.pw.inWin?" · 当前在窗口内":" · 当前窗口外（闲置 5 分钟后自动深睡）");}}
async function loadVoice(){const{code,j}=await api("/voice");if(code!=200||!j)return;
 const tb=$("#vt");tb.innerHTML="";
 j.slots.forEach(s=>{const tr=document.createElement("tr");
  tr.innerHTML="<td>"+s.name+"</td><td class=hex>"+(s.hex||"未绑定")+"</td><td><button data-learn="+s.code+">学</button> <button data-del="+s.code+">清</button></td>";
  tb.appendChild(tr);});
 tb.querySelectorAll("[data-learn]").forEach(b=>b.onclick=async()=>{
  const{code:c,j:x}=await api("/voiceLearn?code="+b.dataset.learn,{method:"POST"});
  if(c==200){toast("15 秒内：喊唤醒词 + 说「"+(x&&x.armed?x.armed:"该命令")+"」");let n=0;
   const iv=setInterval(()=>{loadVoice();if(++n>10)clearInterval(iv);},1500);}});
 tb.querySelectorAll("[data-del]").forEach(b=>b.onclick=async()=>{await api("/voiceDel?code="+b.dataset.del,{method:"POST"});loadVoice();});
 $("#vframes").textContent=(j.frames||[]).filter(fx=>fx.hex).map(fx=>fx.hex+"（"+fx.ageSec+"s 前）").join(" ｜ ")||"暂无 —— 唤醒模块喊一嗓子看看";}
$("#vu").addEventListener("toggle",()=>{if($("#vu").open)loadVoice();});
$("#vEn").onchange=async e=>{const{code}=await api("/voiceEn?on="+(e.target.checked?1:0),{method:"POST"});
 if(code!=200){e.target.checked=!e.target.checked;toast("设置失败",1);}else toast(e.target.checked?"语音已启用":"语音已停用");};
$("#pwApply").onclick=applyPw;
async function applyPw(){const en=$("#pwEn").checked?1:0,st=$("#pwS").value||"19:30",ed=$("#pwE").value||"02:30";
 const{code}=await api("/pwSet?en="+en+"&start="+st+"&end="+ed,{method:"POST"});
 if(code==200){toast("已应用（若正处窗口外，闲置 5 分钟后生效）");refresh();}else toast("设置失败",1);}
document.addEventListener("visibilitychange",()=>{if(!document.hidden)refresh();});
refresh(1);
</script>
</body></html>)rawliteral";

// ------------------------------------------------------------
// 固件更新页（D-12；S3 的 HTML 化，从主页页脚进入）
// ------------------------------------------------------------
static const char PAGE_OTA[] PROGMEM = R"rawliteral(<!doctype html>
<html lang=zh-CN><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<meta name=theme-color content="#10141c"><title>固件更新</title>
<style>body{font:16px/1.6 system-ui,"PingFang SC",sans-serif;background:#10141c;color:#eaf0ff;padding:20px;max-width:520px;margin:auto}
h1{font-size:20px}code{color:#37d67a}button{font:inherit;padding:12px 20px;border:0;border-radius:12px;background:#37d67a;color:#04140b;font-weight:700}
button:disabled{opacity:.45}#bar{height:14px;background:#25304a;border-radius:8px;overflow:hidden;margin:14px 0}
#bar i{display:block;height:100%;width:0;background:#37d67a}#msg{font-size:14px;color:#93a1bd;min-height:44px}
.warn{color:#ffb64a;font-size:14px}a{color:#93a1bd}</style></head><body>
<p><a href=/>← 返回控制页</a></p>
<h1>固件更新</h1>
<p>选择 <code>.bin</code> 文件（<code>build_firmware.bat</code> 编译产物），装置会校验并写入备用分区。</p>
<p class=warn>升级期间装置<b>不能执行动作</b>；上传失败或中途断电不影响现有固件。</p>
<input type=file id=f accept=".bin"><br><br>
<button id=g>开始升级</button>
<div id=bar><i></i></div>
<div id=msg>未选择文件</div>
<p class=warn>若新固件在 45 秒内起不来，页面拒绝动作但仍会亮着——重连本热点后到这里<b>重传旧版 .bin</b> 即可救回，无需拆机。</p>
<script>
"use strict";
const f=document.getElementById('f'),g=document.getElementById('g'),m=document.getElementById('msg'),b=document.querySelector('#bar i');
g.onclick=()=>{if(!f.files.length){m.textContent='请先选择 .bin 文件';return;}
 const fd=new FormData();fd.append('bin',f.files[0]);
 const x=new XMLHttpRequest();x.open('POST','/update');
 x.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded*100/e.total);b.style.width=p+'%';m.textContent='上传中 '+p+'%';}};
 x.onload=()=>{g.disabled=false;b.style.width='100%';
  m.textContent=x.status==200?'升级成功，装置重启中，约 10 秒后重连热点 GUANGDENG':'升级失败('+x.status+')：'+(x.responseText||'').slice(0,80)+'。旧固件仍在运行';};
 x.onerror=()=>{g.disabled=false;m.textContent='传输中断，旧固件仍在运行';};
 g.disabled=true;b.style.width='0';m.textContent='准备上传…';x.send(fd);};
</script></body></html>)rawliteral";
