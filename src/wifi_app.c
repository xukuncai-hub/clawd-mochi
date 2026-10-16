#include "wifi_app.h"
#include "alarm_timer.h"
#include "effects.h"
#include "game_plane.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"

static const char *TAG = "WIFI_APP";

#define WIFI_AP_SSID "ClawdMochi-PRO"
#define WIFI_AP_PASS ""  // 开放热点，免密直连，开箱即用

static WebControlBridge g_bridge = {
    .current_effect_idx = 0,
    .num_effects = 18,
    .auto_cycle = true,
    .request_next = false,
    .request_prev = false,
    .request_set_idx = -1,
    .request_toggle_cycle = false,
    .request_rotate = false,
    .game_input_x = 0,
    .game_input_y = 0,
    .game_input_fire = false,
    .game_request_restart = false,
    .is_game_active = false
};

WebControlBridge* wifi_app_get_bridge(void) {
    return &g_bridge;
}

// 嵌入式精美全功能移动端网页 (极客赛博机甲暗色风 + 街机无线手柄)
static const char index_html[] =
"<!DOCTYPE html>"
"<html lang='zh-CN'>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no'>"
"<title>Clawd Mochi PRO 终端控制器</title>"
"<style>"
":root{--bg:#0b0f17;--card:#141c2b;--border:#1e2d42;--cyan:#00f0ff;--magenta:#ff0077;--gold:#ffbe00;--text:#e2e8f0;--sub:#8899ac}"
"*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,sans-serif}"
"body{background:var(--bg);color:var(--text);padding:14px;max-width:540px;margin:0 auto;line-height:1.5}"
"header{text-align:center;padding:16px 0 12px;border-bottom:1px solid var(--border);margin-bottom:16px}"
"h1{font-size:22px;color:var(--cyan);letter-spacing:1px;font-weight:700;display:flex;align-items:center;justify-content:center;gap:8px}"
".badge{display:inline-block;padding:3px 8px;border-radius:12px;font-size:12px;background:#00f0ff22;color:var(--cyan);border:1px solid #00f0ff55;margin-top:6px}"
".card{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:16px;margin-bottom:16px;box-shadow:0 4px 20px #00000040}"
".card-title{font-size:15px;color:var(--gold);font-weight:600;margin-bottom:12px;display:flex;align-items:center;gap:6px}"
".clock-num{font-size:38px;font-weight:800;color:var(--cyan);text-align:center;font-variant-numeric:tabular-nums;letter-spacing:2px;text-shadow:0 0 12px #00f0ff44}"
".btn{display:inline-flex;align-items:center;justify-content:center;padding:11px 18px;border-radius:10px;font-size:14px;font-weight:600;border:none;cursor:pointer;transition:all .15s ease}"
".btn:active{transform:scale(0.97)}"
".btn-cyan{background:linear-gradient(135deg,#00f0ff,#0099ff);color:#000}"
".btn-magenta{background:linear-gradient(135deg,#ff0077,#b80055);color:#fff}"
".btn-gold{background:linear-gradient(135deg,#ffbe00,#d99b00);color:#000}"
".btn-outline{background:transparent;border:1px solid var(--border);color:var(--text)}"
".btn-outline:active{background:#ffffff11}"
".btn-block{width:100%;margin-top:10px}"
".grid-2{display:grid;grid-template-columns:1fr 1fr;gap:10px}"
".grid-3{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}"
".grid-4{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}"
"input[type='time'],input[type='number']{background:#0b0f17;border:1px solid var(--border);color:#fff;padding:10px;border-radius:8px;font-size:18px;width:100%;text-align:center;outline:none}"
"input:focus{border-color:var(--cyan)}"
".timer-display{font-size:48px;font-weight:800;color:var(--gold);text-align:center;font-variant-numeric:tabular-nums;letter-spacing:2px;padding:8px 0}"
".pill-tag{font-size:12px;padding:7px;border-radius:8px;background:#1e2d42;color:var(--text);border:1px solid var(--border);cursor:pointer;text-align:center;font-weight:500}"
".pill-tag:active{background:#00f0ff33;border-color:var(--cyan)}"
".alert-box{background:linear-gradient(135deg,#ff0055,#990033);padding:14px;border-radius:12px;margin-bottom:16px;text-align:center;display:none;animation:pulse 1s infinite alternate}"
"@keyframes pulse{from{box-shadow:0 0 10px #ff005588}to{box-shadow:0 0 25px #ff0055ff}}"
".switch-row{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}"
".switch{position:relative;width:50px;height:26px;display:inline-block}"
".switch input{opacity:0;width:0;height:0}"
".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#2a3b50;border-radius:26px;transition:.25s}"
".slider:before{position:absolute;content:'';height:20px;width:20px;left:3px;bottom:3px;background-color:white;border-radius:50%;transition:.25s}"
"input:checked+.slider{background-color:var(--cyan)}"
"input:checked+.slider:before{transform:translateX(24px)}"
".fx-btn{font-size:12px;padding:8px 4px;border-radius:8px;background:#1e2d42;color:var(--text);border:1px solid var(--border);cursor:pointer;text-align:center;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
".fx-btn.active{background:#00f0ff22;border-color:var(--cyan);color:var(--cyan);font-weight:700}"

"/* 街机无线手柄样式 */"
".gamepad-box{background:linear-gradient(135deg,#121b2d,#1a132b);border:2px solid #00f0ff55;border-radius:16px;padding:16px;box-shadow:0 0 25px #00f0ff22;touch-action:none;user-select:none;-webkit-user-select:none}"
".pad-btn{display:flex;align-items:center;justify-content:center;font-size:24px;font-weight:700;background:#1a2538;color:#00f0ff;border:2px solid #00f0ff44;border-radius:12px;cursor:pointer;touch-action:none;user-select:none;-webkit-user-select:none;transition:all .08s}"
".pad-btn:active,.pad-btn.pressed{background:#00f0ff;color:#000;box-shadow:0 0 20px #00f0ff;transform:scale(0.92)}"
".btn-fire{background:linear-gradient(135deg,#ff0055,#b80033);border:2px solid #ff0077;color:#fff;font-size:20px;font-weight:900;border-radius:50%;width:86px;height:86px;box-shadow:0 0 20px #ff005588;display:flex;align-items:center;justify-content:center;cursor:pointer;touch-action:none;user-select:none;-webkit-user-select:none}"
".btn-fire:active,.btn-fire.pressed{background:#ff0077;transform:scale(0.90);box-shadow:0 0 35px #ff0077}"
"</style>"
"</head>"
"<body>"

"<div id='alertBox' class='alert-box'>"
"<div style='font-size:18px;font-weight:700;'>🚨 响铃警报进行中！</div>"
"<div id='alertReason' style='font-size:14px;margin:4px 0 10px;'>闹钟/定时器已到期</div>"
"<button onclick='dismissRing()' class='btn btn-cyan btn-block' style='font-size:16px;padding:12px;'>🔕 立即关闭响铃</button>"
"</div>"

"<header>"
"<h1><span>Clawd Mochi</span><span style='color:#fff;font-size:15px;background:#ff0077;padding:2px 6px;border-radius:6px;'>PRO</span></h1>"
"<div class='badge'>WiFi 已直连 · 192.168.4.1</div>"
"</header>"

"<!-- 0. 街机太空飞机大战无线手柄 (重点推荐) -->"
"<div class='card' style='border-color:var(--cyan);background:linear-gradient(135deg,#131b2e,#1b142d);'>"
"<div style='display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;'>"
"<div style='font-size:16px;font-weight:700;color:var(--cyan);display:flex;align-items:center;gap:6px;'>"
"<span>🕹️</span><span>街机飞机大战 · 无线手柄</span>"
"</div>"
"<button onclick='toggleGamePad()' id='btnGameToggle' class='btn btn-cyan' style='padding:6px 12px;font-size:13px;'>▶ 展开手柄</button>"
"</div>"
"<div id='gamePadBox' style='display:none;' class='gamepad-box'>"
"<div style='display:flex;justify-content:space-between;align-items:center;background:#0b0f17;padding:8px 12px;border-radius:8px;border:1px solid var(--border);margin-bottom:14px;font-size:13px;'>"
"<div>❤️ 生命: <b id='gameLives' style='color:#ff0055;'>3</b></div>"
"<div>🏆 得分: <b id='gameScore' style='color:var(--gold);'>00000</b></div>"
"<div>⚡ 武器: <b id='gameLevel' style='color:var(--cyan);'>LV1</b></div>"
"</div>"
"<!-- D-Pad 十字方向键与开火按钮 -->"
"<div style='display:flex;justify-content:space-around;align-items:center;padding:6px 0;'>"
"<div style='display:grid;grid-template-columns:repeat(3,52px);grid-template-rows:repeat(3,52px);gap:6px;'>"
"<div></div>"
"<div class='pad-btn' id='padUp'>▲</div>"
"<div></div>"
"<div class='pad-btn' id='padLeft'>◀</div>"
"<div style='background:#0d1420;border-radius:8px;display:flex;align-items:center;justify-content:center;color:#445566;font-size:11px;'>PAD</div>"
"<div class='pad-btn' id='padRight'>▶</div>"
"<div></div>"
"<div class='pad-btn' id='padDown'>▼</div>"
"<div></div>"
"</div>"
"<div style='display:flex;flex-direction:column;align-items:center;gap:6px;'>"
"<div class='btn-fire' id='padFire'>FIRE</div>"
"<span style='font-size:11px;color:var(--sub);'>按住连发</span>"
"</div>"
"</div>"
"<div class='grid-2' style='margin-top:14px;'>"
"<button onclick='restartGame()' class='btn btn-outline'>🔄 重新开始</button>"
"<button onclick='exitGame()' class='btn btn-outline' style='color:#ff6688;border-color:#ff005544;'>❌ 退出手柄</button>"
"</div>"
"<div style='font-size:11px;color:var(--sub);text-align:center;margin-top:8px;'>提示：电脑端可直接使用键盘 方向键 / WASD 移动，空格键 开火</div>"
"</div>"
"</div>"

"<!-- 1. 实时时钟看板 -->"
"<div class='card'>"
"<div class='card-title'><span>🕒</span><span>设备实时时钟 (自动同步)</span></div>"
"<div class='clock-num' id='liveClock'>--:--:--</div>"
"<div style='text-align:center;font-size:12px;color:var(--sub);margin-top:6px;' id='timeSyncTip'>已连接设备并保持毫秒级校准</div>"
"<div style='text-align:center;margin-top:8px;'><button onclick='syncPhoneTime()' class='btn btn-outline' style='padding:6px 14px;font-size:12px;'>🔄 立即校准为当前手机时间</button></div>"
"</div>"

"<!-- 2. 智能闹钟 -->"
"<div class='card'>"
"<div class='card-title'><span>⏰</span><span>日常闹钟设定</span></div>"
"<div class='switch-row'>"
"<span style='font-size:14px;color:var(--text);'>启用每日闹钟提醒</span>"
"<label class='switch'><input type='checkbox' id='alarmEnable' onchange='saveAlarm()'><span class='slider'></span></label>"
"</div>"
"<div style='margin:12px 0;'>"
"<input type='time' id='alarmTime' value='07:30' onchange='saveAlarm()'>"
"</div>"
"<button onclick='saveAlarm()' class='btn btn-cyan btn-block'>💾 保存闹钟设置</button>"
"<div id='alarmStatusText' style='font-size:12px;color:var(--cyan);margin-top:8px;text-align:center;'>闹钟状态检测中...</div>"
"</div>"

"<!-- 3. 倒计时定时器 -->"
"<div class='card'>"
"<div class='card-title'><span>⏱️</span><span>倒计时定时器 / 番茄钟</span></div>"
"<div class='timer-display' id='timerNum'>00:00</div>"
"<div style='font-size:12px;color:var(--sub);text-align:center;margin-bottom:12px;'>快捷预设倒计时时长：</div>"
"<div class='grid-4' style='margin-bottom:12px;'>"
"<div class='pill-tag' onclick='setTimerPreset(60)'>1 分钟</div>"
"<div class='pill-tag' onclick='setTimerPreset(180)'>3 分钟</div>"
"<div class='pill-tag' onclick='setTimerPreset(300)'>5 分钟</div>"
"<div class='pill-tag' onclick='setTimerPreset(600)'>10 分钟</div>"
"</div>"
"<div class='grid-2' style='margin-bottom:14px;'>"
"<div class='pill-tag' onclick='setTimerPreset(1500)'>🍅 25 分钟番茄钟</div>"
"<div class='pill-tag' onclick='setTimerPreset(2700)'>🧘 45 分钟深度学习</div>"
"</div>"
"<div class='grid-2' style='margin-bottom:12px;'>"
"<input type='number' id='customMins' placeholder='分' min='0' max='180'>"
"<input type='number' id='customSecs' placeholder='秒' min='0' max='59'>"
"</div>"
"<div class='grid-3'>"
"<button onclick='startCustomTimer()' class='btn btn-gold'>▶ 开始</button>"
"<button onclick='pauseResumeTimer()' id='btnPause' class='btn btn-outline'>⏸ 暂停</button>"
"<button onclick='stopTimer()' class='btn btn-magenta'>⏹ 复位</button>"
"</div>"
"</div>"

"<!-- 4. 远程画面与特效遥控器 -->"
"<div class='card'>"
"<div class='card-title'><span>🎨</span><span>远程控制与动效切换</span></div>"
"<div style='font-size:12px;color:var(--sub);margin-bottom:8px;'>当前播放特效：<b id='curFxName' style='color:var(--cyan);'>加载中...</b></div>"
"<div class='grid-2' style='margin-bottom:12px;'>"
"<button onclick='toggleCycle()' id='btnCycle' class='btn btn-outline'>🔄 自动轮播: 开</button>"
"<button onclick='rotateScreen()' class='btn btn-outline'>🔁 旋转屏幕 90°</button>"
"</div>"
"<div class='grid-3' id='fxListGrid'>"
"<!-- 18 大特效远程点击按钮动态渲染 -->"
"</div>"
"</div>"

"<script>"
"const FX_NAMES=['甜甜圈','赛博落日','数字雨','等离子','音频频谱','引力黑洞','银河旋涡','毁灭火焰','虫洞隧道','星空超立方','DNA双螺旋','熔岩融球','战术雷达','激光示波器','生命游戏','北极极光','朱利亚分形','飞机大战🎮'];"
"let gTimerState='idle';"
"let gGamePadOpen=false;"
"let gInputState={x:0,y:0,fire:false};"
"let gInputInterval=null;"

"function initFxButtons(){"
"const grid=document.getElementById('fxListGrid');"
"grid.innerHTML='';"
"FX_NAMES.forEach((name,i)=>{"
"const btn=document.createElement('div');"
"btn.className='fx-btn';"
"btn.id='fxBtn_'+i;"
"btn.innerText=(i+1)+'. '+name;"
"btn.onclick=()=>setFx(i);"
"grid.appendChild(btn);"
"});"
"}"

"function toggleGamePad(){"
"const box=document.getElementById('gamePadBox');"
"const btn=document.getElementById('btnGameToggle');"
"gGamePadOpen=!gGamePadOpen;"
"if(gGamePadOpen){"
"box.style.display='block';"
"btn.innerText='▼ 收起手柄';"
"fetch('/api/game_control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'enter'})});"
"startInputLoop();"
"}else{"
"box.style.display='none';"
"btn.innerText='▶ 展开手柄';"
"stopInputLoop();"
"}"
"}"

"function restartGame(){"
"fetch('/api/game_control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'restart'})});"
"}"

"function exitGame(){"
"gGamePadOpen=false;"
"document.getElementById('gamePadBox').style.display='none';"
"document.getElementById('btnGameToggle').innerText='▶ 展开手柄';"
"stopInputLoop();"
"fetch('/api/game_control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'exit'})});"
"}"

"function sendGameInput(){"
"fetch('/api/game_input',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(gInputState)}).catch(e=>{});"
"}"

"function startInputLoop(){"
"if(!gInputInterval){"
"gInputInterval=setInterval(()=>{if(gGamePadOpen&&(gInputState.x!==0||gInputState.y!==0||gInputState.fire)){sendGameInput();}},60);"
"}"
"}"

"function stopInputLoop(){"
"if(gInputInterval){clearInterval(gInputInterval);gInputInterval=null;}"
"gInputState={x:0,y:0,fire:false};"
"sendGameInput();"
"}"

"function setupPadTouch(elemId,onDown,onUp){"
"const el=document.getElementById(elemId);"
"if(!el)return;"
"const down=e=>{e.preventDefault();el.classList.add('pressed');if(navigator.vibrate)navigator.vibrate(15);onDown();sendGameInput();};"
"const up=e=>{e.preventDefault();el.classList.remove('pressed');onUp();sendGameInput();};"
"el.addEventListener('touchstart',down,{passive:false});"
"el.addEventListener('touchend',up,{passive:false});"
"el.addEventListener('touchcancel',up,{passive:false});"
"el.addEventListener('mousedown',down);"
"el.addEventListener('mouseup',up);"
"el.addEventListener('mouseleave',up);"
"}"

"function initGamePadControls(){"
"setupPadTouch('padLeft',()=>{gInputState.x=-1;},()=>{gInputState.x=0;});"
"setupPadTouch('padRight',()=>{gInputState.x=1;},()=>{gInputState.x=0;});"
"setupPadTouch('padUp',()=>{gInputState.y=-1;},()=>{gInputState.y=0;});"
"setupPadTouch('padDown',()=>{gInputState.y=1;},()=>{gInputState.y=0;});"
"setupPadTouch('padFire',()=>{gInputState.fire=true;},()=>{gInputState.fire=false;});"

"window.addEventListener('keydown',e=>{"
"if(!gGamePadOpen)return;"
"if(e.key==='ArrowLeft'||e.key==='a'||e.key==='A'){gInputState.x=-1;sendGameInput();}"
"else if(e.key==='ArrowRight'||e.key==='d'||e.key==='D'){gInputState.x=1;sendGameInput();}"
"else if(e.key==='ArrowUp'||e.key==='w'||e.key==='W'){gInputState.y=-1;sendGameInput();}"
"else if(e.key==='ArrowDown'||e.key==='s'||e.key==='S'){gInputState.y=1;sendGameInput();}"
"else if(e.key===' '||e.key==='j'||e.key==='J'){gInputState.fire=true;sendGameInput();}"
"else if(e.key==='r'||e.key==='R'){restartGame();}"
"});"

"window.addEventListener('keyup',e=>{"
"if(!gGamePadOpen)return;"
"if(e.key==='ArrowLeft'||e.key==='a'||e.key==='A'||e.key==='ArrowRight'||e.key==='d'||e.key==='D'){gInputState.x=0;sendGameInput();}"
"if(e.key==='ArrowUp'||e.key==='w'||e.key==='W'||e.key==='ArrowDown'||e.key==='s'||e.key==='S'){gInputState.y=0;sendGameInput();}"
"if(e.key===' '||e.key==='j'||e.key==='J'){gInputState.fire=false;sendGameInput();}"
"});"
"}"

"function syncPhoneTime(){"
"const now=new Date();"
"fetch('/api/sync_time',{"
"method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({"
"y:now.getFullYear(),"
"mon:now.getMonth()+1,"
"d:now.getDate(),"
"h:now.getHours(),"
"m:now.getMinutes(),"
"s:now.getSeconds(),"
"ts:Math.floor(now.getTime()/1000)"
"})"
"}).then(r=>r.json()).then(()=>{"
"document.getElementById('timeSyncTip').innerText='✅ 时间已同手机实时校准';"
"updateStatus();"
"}).catch(e=>{});"
"}"

"function updateStatus(){"
"fetch('/api/status').then(r=>r.json()).then(d=>{"
"document.getElementById('liveClock').innerText=d.time||'--:--:--';"
"const alertBox=document.getElementById('alertBox');"
"if(d.is_ringing){"
"alertBox.style.display='block';"
"document.getElementById('alertReason').innerText=d.ring_title||'闹钟/定时器已响铃！';"
"}else{alertBox.style.display='none';}"
"const ab=document.getElementById('alarmEnable');"
"if(document.activeElement!==ab){ab.checked=d.alarm_enabled;}"
"const at=document.getElementById('alarmTime');"
"const hh=String(d.alarm_h).padStart(2,'0');"
"const mm=String(d.alarm_m).padStart(2,'0');"
"if(document.activeElement!==at&&d.alarm_h!==undefined){at.value=hh+':'+mm;}"
"document.getElementById('alarmStatusText').innerText=d.alarm_enabled?('✅ 闹钟已就绪: 每天 '+hh+':'+mm+' 响铃'):'⚪ 闹钟未开启';"
"gTimerState=d.timer_state;"
"const tm=Math.floor(d.timer_rem/60);"
"const ts=d.timer_rem%60;"
"document.getElementById('timerNum').innerText=String(tm).padStart(2,'0')+':'+String(ts).padStart(2,'0');"
"const btnP=document.getElementById('btnPause');"
"if(gTimerState==='running'){btnP.innerText='⏸ 暂停';btnP.className='btn btn-outline';}"
"else if(gTimerState==='paused'){btnP.innerText='▶ 继续';btnP.className='btn btn-gold';}"
"else{btnP.innerText='⏸ 暂停';btnP.className='btn btn-outline';}"
"document.getElementById('curFxName').innerText='['+(d.cur_fx+1)+'/18] '+(FX_NAMES[d.cur_fx]||d.cur_fx_title);"
"document.getElementById('btnCycle').innerText='🔄 自动轮播: '+(d.auto_cycle?'开':'关');"
"document.querySelectorAll('.fx-btn').forEach((b,idx)=>{if(idx===d.cur_fx){b.classList.add('active');}else{b.classList.remove('active');}});"

"if(d.game_score!==undefined){"
"document.getElementById('gameScore').innerText=String(d.game_score).padStart(5,'0');"
"document.getElementById('gameLives').innerText=d.game_lives;"
"document.getElementById('gameLevel').innerText='LV'+d.game_level;"
"}"

"if(d.cur_fx===17&&!gGamePadOpen){"
"gGamePadOpen=true;"
"document.getElementById('gamePadBox').style.display='block';"
"document.getElementById('btnGameToggle').innerText='▼ 收起手柄';"
"startInputLoop();"
"}"
"}).catch(e=>{});"
"}"

"function saveAlarm(){"
"const enabled=document.getElementById('alarmEnable').checked;"
"const val=document.getElementById('alarmTime').value||'07:30';"
"const parts=val.split(':');"
"const h=parseInt(parts[0],10)||0;"
"const m=parseInt(parts[1],10)||0;"
"fetch('/api/set_alarm',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:enabled,hour:h,minute:m})}).then(()=>updateStatus());"
"}"

"function setTimerPreset(sec){"
"fetch('/api/timer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'start',seconds:sec})}).then(()=>updateStatus());"
"}"

"function startCustomTimer(){"
"const m=parseInt(document.getElementById('customMins').value,10)||0;"
"const s=parseInt(document.getElementById('customSecs').value,10)||0;"
"const total=m*60+s;"
"if(total>0){setTimerPreset(total);}"
"}"

"function pauseResumeTimer(){"
"if(gTimerState==='running'){"
"fetch('/api/timer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'pause'})}).then(()=>updateStatus());"
"}else if(gTimerState==='paused'){"
"fetch('/api/timer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'resume'})}).then(()=>updateStatus());"
"}"
"}"

"function stopTimer(){"
"fetch('/api/timer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'stop'})}).then(()=>updateStatus());"
"}"

"function dismissRing(){"
"fetch('/api/dismiss',{method:'POST'}).then(()=>updateStatus());"
"}"

"function setFx(idx){"
"if(idx===17){"
"toggleGamePad();"
"}else{"
"fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'set_fx',idx:idx})}).then(()=>updateStatus());"
"}"
"}"

"function toggleCycle(){"
"fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'toggle_cycle'})}).then(()=>updateStatus());"
"}"

"function rotateScreen(){"
"fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'rotate'})}).then(()=>updateStatus());"
"}"

"window.onload=function(){"
"initFxButtons();"
"initGamePadControls();"
"syncPhoneTime();"
"updateStatus();"
"setInterval(updateStatus,1000);"
"};"
"</script>"
"</body>"
"</html>";

// GET / - 返回移动端完整网页
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
}

// GET /api/status - 返回系统时间、闹钟、定时器及屏幕与游戏状态
static esp_err_t api_status_handler(httpd_req_t *req) {
    AlarmTimerContext *ctx = alarm_timer_get_ctx();
    time_t now_t = time(NULL);
    struct tm cur_tm;
    localtime_r(&now_t, &cur_tm);

    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", cur_tm.tm_hour, cur_tm.tm_min, cur_tm.tm_sec);

    const char *tstate_str = "idle";
    if (ctx->timer_state == TIMER_STATE_RUNNING) tstate_str = "running";
    else if (ctx->timer_state == TIMER_STATE_PAUSED) tstate_str = "paused";
    else if (ctx->timer_state == TIMER_STATE_RINGING) tstate_str = "ringing";

    int g_score = 0, g_high_score = 0, g_lives = 3, g_level = 1;
    bool g_gameover = false;
    game_plane_get_status(&g_score, &g_high_score, &g_lives, &g_level, &g_gameover);

    char json_buf[480];
    snprintf(json_buf, sizeof(json_buf),
        "{"
        "\"time\":\"%s\","
        "\"time_synced\":%s,"
        "\"alarm_enabled\":%s,"
        "\"alarm_h\":%d,"
        "\"alarm_m\":%d,"
        "\"timer_state\":\"%s\","
        "\"timer_rem\":%d,"
        "\"timer_tot\":%d,"
        "\"is_ringing\":%s,"
        "\"ring_title\":\"%s\","
        "\"cur_fx\":%d,"
        "\"cur_fx_title\":\"%s\","
        "\"auto_cycle\":%s,"
        "\"game_score\":%d,"
        "\"game_high\":%d,"
        "\"game_lives\":%d,"
        "\"game_level\":%d,"
        "\"game_over\":%s"
        "}",
        time_str,
        ctx->time_synced ? "true" : "false",
        ctx->alarm_enabled ? "true" : "false",
        ctx->alarm_hour,
        ctx->alarm_minute,
        tstate_str,
        ctx->timer_remaining_sec,
        ctx->timer_total_sec,
        ctx->is_ringing ? "true" : "false",
        ctx->ring_title,
        g_bridge.current_effect_idx,
        (g_bridge.current_effect_idx == 17) ? "街机飞机大战" : "特效展示",
        g_bridge.auto_cycle ? "true" : "false",
        g_score,
        g_high_score,
        g_lives,
        g_level,
        g_gameover ? "true" : "false"
    );

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
}

// POST /api/game_input - 接收手机端手柄按键信号
static esp_err_t api_game_input_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        int x = 0, y = 0;
        bool fire = false;
        char *px = strstr(buf, "\"x\":");
        char *py = strstr(buf, "\"y\":");
        char *pf = strstr(buf, "\"fire\":");
        if (px) sscanf(px, "\"x\":%d", &x);
        if (py) sscanf(py, "\"y\":%d", &y);
        if (pf) fire = (strstr(pf, "true") != NULL || strstr(pf, "1") != NULL);

        g_bridge.game_input_x = x;
        g_bridge.game_input_y = y;
        g_bridge.game_input_fire = fire;
        g_bridge.is_game_active = true;

        if (g_bridge.current_effect_idx != 17) {
            g_bridge.request_set_idx = 17;
            g_bridge.auto_cycle = false;
        }
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// POST /api/game_control - 控制游戏进入、重新开始、退出
static esp_err_t api_game_control_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        if (strstr(buf, "\"action\":\"enter\"") || strstr(buf, "\"action\":\"start\"")) {
            g_bridge.request_set_idx = 17;
            g_bridge.is_game_active = true;
            g_bridge.auto_cycle = false;
            g_bridge.game_request_restart = true;
        } else if (strstr(buf, "\"action\":\"restart\"")) {
            g_bridge.game_request_restart = true;
        } else if (strstr(buf, "\"action\":\"exit\"")) {
            g_bridge.is_game_active = false;
            g_bridge.request_set_idx = 0;
            g_bridge.auto_cycle = true;
        }
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// POST /api/sync_time - 手机端一键对时
static esp_err_t api_sync_time_handler(httpd_req_t *req) {
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        int y = 0, mon = 0, d = 0, h = -1, m = -1, s = -1;
        long long ts = 0;

        char *py = strstr(buf, "\"y\":");
        char *pmon = strstr(buf, "\"mon\":");
        char *pd = strstr(buf, "\"d\":");
        char *ph = strstr(buf, "\"h\":");
        char *pm = strstr(buf, "\"m\":");
        char *ps = strstr(buf, "\"s\":");
        char *pts = strstr(buf, "\"ts\":");

        if (py) sscanf(py, "\"y\":%d", &y);
        if (pmon) sscanf(pmon, "\"mon\":%d", &mon);
        if (pd) sscanf(pd, "\"d\":%d", &d);
        if (ph) sscanf(ph, "\"h\":%d", &h);
        if (pm) sscanf(pm, "\"m\":%d", &m);
        if (ps) sscanf(ps, "\"s\":%d", &s);
        if (pts) sscanf(pts, "\"ts\":%lld", &ts);

        if (y >= 2024 && h >= 0 && m >= 0 && s >= 0) {
            alarm_timer_set_datetime(y, mon, d, h, m, s);
        } else if (ts > 1000000000LL) {
            alarm_timer_set_timestamp((time_t)ts);
        }
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// POST /api/set_alarm - 设置日常闹钟
static esp_err_t api_set_alarm_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        bool enabled = (strstr(buf, "\"enabled\":true") != NULL);
        int h = 7, m = 30;
        char *ph = strstr(buf, "\"hour\":");
        char *pm = strstr(buf, "\"minute\":");
        if (ph) sscanf(ph, "\"hour\":%d", &h);
        if (pm) sscanf(pm, "\"minute\":%d", &m);
        alarm_timer_set_alarm(enabled, h, m);
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// POST /api/timer - 控制倒计时定时器
static esp_err_t api_timer_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        if (strstr(buf, "\"action\":\"start\"")) {
            int sec = 300;
            char *ps = strstr(buf, "\"seconds\":");
            if (ps) sscanf(ps, "\"seconds\":%d", &sec);
            alarm_timer_start(sec);
        } else if (strstr(buf, "\"action\":\"pause\"")) {
            alarm_timer_pause();
        } else if (strstr(buf, "\"action\":\"resume\"")) {
            alarm_timer_resume();
        } else if (strstr(buf, "\"action\":\"stop\"")) {
            alarm_timer_stop();
        }
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// POST /api/dismiss - 关闭响铃警报
static esp_err_t api_dismiss_handler(httpd_req_t *req) {
    alarm_timer_dismiss_ring();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// POST /api/control - 远程屏幕控制
static esp_err_t api_control_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        if (strstr(buf, "\"action\":\"set_fx\"")) {
            int idx = 0;
            char *pi = strstr(buf, "\"idx\":");
            if (pi && sscanf(pi, "\"idx\":%d", &idx) == 1) {
                g_bridge.request_set_idx = idx;
            }
        } else if (strstr(buf, "\"action\":\"toggle_cycle\"")) {
            g_bridge.request_toggle_cycle = true;
        } else if (strstr(buf, "\"action\":\"rotate\"")) {
            g_bridge.request_rotate = true;
        }
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// Captive Portal 重定向处理器 (手机连入自动弹出或探测联网时跳转主页)
static esp_err_t captive_portal_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// 嵌入式简易 DNS 服务器：将所有域名请求解析为 192.168.4.1 (实现免外网强制门户)
static void dns_server_task(void *pvParameters) {
    struct sockaddr_in server_addr;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket creation failed");
        vTaskDelete(NULL);
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(53);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "DNS socket bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Captive DNS server running on port 53");

    uint8_t rx_buffer[256];
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);
        if (len > 12) {
            rx_buffer[2] |= 0x80; // QR = 1 (Response)
            rx_buffer[3] = 0x80; // RA = 1
            rx_buffer[7] = 1;    // ANCOUNT = 1

            int q_end = 12;
            while (q_end < len && rx_buffer[q_end] != 0) {
                q_end += rx_buffer[q_end] + 1;
            }
            q_end += 5;

            if (q_end <= len && q_end + 16 < sizeof(rx_buffer)) {
                rx_buffer[q_end++] = 0xC0;
                rx_buffer[q_end++] = 0x0C;
                rx_buffer[q_end++] = 0x00; // TYPE A
                rx_buffer[q_end++] = 0x01;
                rx_buffer[q_end++] = 0x00; // CLASS IN
                rx_buffer[q_end++] = 0x01;
                rx_buffer[q_end++] = 0x00; // TTL: 60s
                rx_buffer[q_end++] = 0x00;
                rx_buffer[q_end++] = 0x00;
                rx_buffer[q_end++] = 0x3C;
                rx_buffer[q_end++] = 0x00; // RDLENGTH: 4
                rx_buffer[q_end++] = 0x04;
                rx_buffer[q_end++] = 192;  // 192.168.4.1
                rx_buffer[q_end++] = 168;
                rx_buffer[q_end++] = 4;
                rx_buffer[q_end++] = 1;

                sendto(sock, rx_buffer, q_end, 0, (struct sockaddr *)&source_addr, socklen);
            }
        }
    }
}

// Wi-Fi 及网络事件监视
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, ">>> [WIFI] 手机已连入热点! MAC: %02x:%02x:%02x:%02x:%02x:%02x, AID: %d",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5], event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGW(TAG, ">>> [WIFI] 手机断开连接! MAC: %02x:%02x:%02x:%02x:%02x:%02x, AID: %d, 原因: %d",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5], event->aid, event->reason);
    } else if (event_base == IP_EVENT) {
        ESP_LOGI(TAG, ">>> [DHCP] 手机成功获取 IP 地址并接入网络");
    }
}

static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    // 强制门户重定向至主页
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 20;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = api_status_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t game_in_uri = { .uri = "/api/game_input", .method = HTTP_POST, .handler = api_game_input_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &game_in_uri);

        httpd_uri_t game_ctrl_uri = { .uri = "/api/game_control", .method = HTTP_POST, .handler = api_game_control_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &game_ctrl_uri);

        httpd_uri_t sync_uri = { .uri = "/api/sync_time", .method = HTTP_POST, .handler = api_sync_time_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &sync_uri);

        httpd_uri_t alarm_uri = { .uri = "/api/set_alarm", .method = HTTP_POST, .handler = api_set_alarm_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &alarm_uri);

        httpd_uri_t timer_uri = { .uri = "/api/timer", .method = HTTP_POST, .handler = api_timer_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &timer_uri);

        httpd_uri_t dismiss_uri = { .uri = "/api/dismiss", .method = HTTP_POST, .handler = api_dismiss_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &dismiss_uri);

        httpd_uri_t ctrl_uri = { .uri = "/api/control", .method = HTTP_POST, .handler = api_control_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &ctrl_uri);

        // 手机连接探测重定向
        httpd_uri_t cp1 = { .uri = "/generate_204", .method = HTTP_GET, .handler = captive_portal_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &cp1);
        httpd_uri_t cp2 = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = captive_portal_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &cp2);

        // 404 捕获所有未知 URL 并重定向至控制主页
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

        return server;
    }
    ESP_LOGE(TAG, "Error starting server!");
    return NULL;
}

void wifi_app_init(void) {
    ESP_LOGI(TAG, "Initializing NVS, Wi-Fi SoftAP & Web Server...");

    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化 TCP/IP 网络堆栈
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建 AP netif (默认网关与静态 IP 均为 192.168.4.1，DHCP 服务器自动管理)
    esp_netif_create_default_wifi_ap();

    // 注册网络事件监视器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_ASSIGNED_IP_TO_CLIENT, &wifi_event_handler, NULL, NULL));

    // 3. 配置并启动 Wi-Fi SoftAP 热点模式 (免密开放热点 + 强制门户，杜绝一切手机鉴权拒绝)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    if (strlen(WIFI_AP_PASS) > 0) {
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char *)wifi_config.ap.password, WIFI_AP_PASS, sizeof(wifi_config.ap.password));
        wifi_config.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW20));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 禁用 Wi-Fi 省电模式，确保天线最大射频功率与手机稳定长连接
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "  Wi-Fi AP Started! SSID: [%s]", WIFI_AP_SSID);
    ESP_LOGI(TAG, "  Wi-Fi Auth Mode       : [%s]", (wifi_config.ap.authmode == WIFI_AUTH_OPEN) ? "OPEN (免密直连)" : "WPA2-PSK");
    if (strlen(WIFI_AP_PASS) > 0) {
        ESP_LOGI(TAG, "  Wi-Fi Password        : [%s]", WIFI_AP_PASS);
    }
    ESP_LOGI(TAG, "  Web Control URL       : http://192.168.4.1");
    ESP_LOGI(TAG, "==================================================");

    // 4. 启动轻量级 HTTP Web 服务器
    start_webserver();

    // 5. 启动简易 DNS 强制门户解析服务 (UDP 53)
    xTaskCreate(dns_server_task, "dns_server", 3072, NULL, 3, NULL);
}
