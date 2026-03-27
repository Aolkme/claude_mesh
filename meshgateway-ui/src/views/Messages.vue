<template>
  <div class="messages-view">
    <!-- 发送文本 -->
    <div class="card">
      <div class="card-header">发送文本消息</div>
      <div class="card-body">
        <div class="form-row">
          <input v-model="textTo" placeholder="收件人: !aabbccdd 或 broadcast" class="input" style="flex:1">
          <input v-model.number="textCh" type="number" placeholder="信道" class="input" style="width:70px">
          <label class="check-label"><input v-model="textAck" type="checkbox"> ACK</label>
        </div>
        <div class="form-row">
          <input v-model="textMsg" placeholder="消息内容..." class="input" style="flex:1" @keyup.enter="sendText">
          <button @click="sendText" class="btn btn-primary" :disabled="!connected || !textTo || !textMsg">发送</button>
        </div>
        <div v-if="textResult" class="result" :class="textResult.status === 'ok' ? 'ok' : 'err'">
          {{ JSON.stringify(textResult) }}
        </div>
      </div>
    </div>

    <!-- 发送位置 -->
    <div class="card" style="margin-top:12px">
      <div class="card-header">发送位置</div>
      <div class="card-body">
        <div class="form-row">
          <input v-model="posTo" placeholder="收件人" class="input" style="width:180px">
          <input v-model.number="posLat" type="number" step="0.00001" placeholder="纬度" class="input" style="width:110px">
          <input v-model.number="posLon" type="number" step="0.00001" placeholder="经度" class="input" style="width:110px">
          <input v-model.number="posAlt" type="number" placeholder="高度(m)" class="input" style="width:90px">
          <input v-model.number="posCh" type="number" placeholder="信道" class="input" style="width:70px">
          <button @click="sendPos" class="btn btn-primary" :disabled="!connected || !posTo">发送位置</button>
        </div>
        <div v-if="posResult" class="result" :class="posResult.status === 'ok' ? 'ok' : 'err'">
          {{ JSON.stringify(posResult) }}
        </div>
      </div>
    </div>

    <!-- Admin 操作 -->
    <div class="card" style="margin-top:12px">
      <div class="card-header">Admin 操作</div>
      <div class="card-body">
        <div class="form-row" style="align-items:center">
          <span class="hint" style="white-space:nowrap">节点 ID:</span>
          <input v-model="adminNode" placeholder="!aabbccdd" class="input" style="width:160px">
          <span class="hint">Passkey:</span>
          <input v-model.number="adminPasskey" type="number" placeholder="0" class="input" style="width:100px">
        </div>
        <div class="form-row" style="flex-wrap:wrap;gap:8px">
          <button @click="adminPasskeyReq" class="btn btn-secondary" :disabled="!connected || !adminNode">获取 Passkey</button>
          <button @click="adminGetConfig" class="btn btn-secondary" :disabled="!connected || !adminNode">读取配置</button>
          <button @click="adminGetChannel" class="btn btn-secondary" :disabled="!connected || !adminNode">读取信道</button>
          <button @click="adminReboot" class="btn btn-danger" :disabled="!connected || !adminNode">重启节点</button>
        </div>
        <div v-if="adminResult" class="result" :class="adminResult.status === 'ok' ? 'ok' : 'err'">
          {{ JSON.stringify(adminResult) }}
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useGateway } from '../composables/useGateway.js'

const { connected, sendCmd } = useGateway()

const textTo = ref(''), textCh = ref(0), textAck = ref(false), textMsg = ref('')
const textResult = ref(null)
async function sendText() {
  textResult.value = await sendCmd({ cmd: 'send_text', to: textTo.value, text: textMsg.value, channel: textCh.value, want_ack: textAck.value })
  if (textResult.value?.status === 'ok') textMsg.value = ''
}

const posTo = ref(''), posLat = ref(null), posLon = ref(null), posAlt = ref(0), posCh = ref(0)
const posResult = ref(null)
async function sendPos() {
  posResult.value = await sendCmd({ cmd: 'send_position', to: posTo.value, lat: posLat.value, lon: posLon.value, alt: posAlt.value, channel: posCh.value })
}

const adminNode = ref(''), adminPasskey = ref(0)
const adminResult = ref(null)
async function adminPasskeyReq() {
  adminResult.value = await sendCmd({ cmd: 'admin_get_session_passkey', node_id: adminNode.value })
}
async function adminGetConfig() {
  adminResult.value = await sendCmd({ cmd: 'admin_get_config', node_id: adminNode.value, config_type: 0, passkey: adminPasskey.value })
}
async function adminGetChannel() {
  adminResult.value = await sendCmd({ cmd: 'admin_get_channel', node_id: adminNode.value, channel_index: 0, passkey: adminPasskey.value })
}
async function adminReboot() {
  if (!confirm(`确认重启节点 ${adminNode.value}？`)) return
  adminResult.value = await sendCmd({ cmd: 'admin_reboot', node_id: adminNode.value, delay_s: 5, passkey: adminPasskey.value })
}
</script>
