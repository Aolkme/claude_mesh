/**
 * useGateway.js — WebSocket 连接到 meshgateway 守护进程
 *
 * 提供：
 *   status     — 网关状态 (reactive)
 *   nodes      — 节点列表 (reactive)
 *   events     — 最近 200 条事件日志 (reactive)
 *   connected  — WebSocket 是否已连接 (ref)
 *   send(cmd)  — 发送 JSON 命令对象，返回 Promise<response>
 */
import { ref, reactive, readonly } from 'vue'

const WS_URL = (() => {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:'
  return `${proto}//${location.host}/ws`
})()

const MAX_EVENTS = 200

// ─── 全局单例状态 ───
const connected = ref(false)
const status = reactive({
  my_node_id: '',
  connected: false,
  config_complete: false,
  node_count: 0,
  rx_count: 0,
  uptime_s: 0,
  serial_device: '',
})
const nodes = ref([])
const events = ref([])

// 待回调的命令 Promise (key = JSON cmd string, rough matching)
const pendingCallbacks = []

let ws = null
let reconnectTimer = null

function addEvent(data, type = 'event') {
  events.value.unshift({ ts: new Date().toISOString().substr(11, 12), data, type })
  if (events.value.length > MAX_EVENTS)
    events.value.length = MAX_EVENTS
}

function handleMessage(raw) {
  let d
  try { d = JSON.parse(raw) } catch { addEvent(raw, 'raw'); return }

  if (d.event) {
    addEvent(d, 'event')
    // 节点变化：重新拉取列表
    if (d.event === 'node_new' || d.event === 'node_updated')
      sendCmd({ cmd: 'get_nodes' })
    if (d.event === 'config_complete' || d.event === 'serial_connected') {
      sendCmd({ cmd: 'get_status' })
      sendCmd({ cmd: 'get_nodes' })
    }
  } else {
    // 响应：更新状态
    addEvent(d, d.status === 'ok' ? 'ok' : 'error')
    if (d.connected !== undefined) Object.assign(status, d)
    if (Array.isArray(d.nodes)) nodes.value = d.nodes

    // resolve 最早的 pending callback
    const cb = pendingCallbacks.shift()
    if (cb) cb(d)
  }
}

function connect() {
  if (ws) { ws.onclose = null; ws.close() }
  ws = new WebSocket(WS_URL)

  ws.onopen = () => {
    connected.value = true
    addEvent({ event: 'ws_connected' }, 'ok')
    sendCmd({ cmd: 'get_status' })
    sendCmd({ cmd: 'get_nodes' })
  }

  ws.onmessage = (e) => handleMessage(e.data)

  ws.onclose = () => {
    connected.value = false
    addEvent({ event: 'ws_disconnected' }, 'error')
    reconnectTimer = setTimeout(connect, 3000)
  }

  ws.onerror = () => addEvent({ event: 'ws_error' }, 'error')
}

/** 发送命令，返回 Promise<response> */
function sendCmd(cmdObj) {
  return new Promise((resolve) => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      resolve({ status: 'error', error: 'not connected' })
      return
    }
    pendingCallbacks.push(resolve)
    ws.send(JSON.stringify(cmdObj))
  })
}

// 自动启动连接
connect()
// 定期刷新状态
setInterval(() => { if (connected.value) sendCmd({ cmd: 'get_status' }) }, 10000)

export function useGateway() {
  return {
    connected: readonly(connected),
    status: readonly(status),
    nodes: readonly(nodes),
    events: readonly(events),
    sendCmd,
  }
}
