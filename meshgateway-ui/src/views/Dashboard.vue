<template>
  <div class="dashboard">
    <!-- 状态卡片 -->
    <div class="stat-grid">
      <div class="card stat-card">
        <div class="stat-label">节点数</div>
        <div class="stat-value">{{ status.node_count }}</div>
      </div>
      <div class="card stat-card">
        <div class="stat-label">接收帧</div>
        <div class="stat-value">{{ status.rx_count }}</div>
      </div>
      <div class="card stat-card">
        <div class="stat-label">运行时长</div>
        <div class="stat-value">{{ formatUptime(status.uptime_s) }}</div>
      </div>
      <div class="card stat-card">
        <div class="stat-label">配置完成</div>
        <div class="stat-value" :class="status.config_complete ? 'text-green' : 'text-yellow'">
          {{ status.config_complete ? '✓' : '…' }}
        </div>
      </div>
    </div>

    <!-- 串口连接控制 -->
    <div class="card">
      <div class="card-header">串口连接</div>
      <div class="card-body">
        <div class="form-row">
          <input v-model="device" placeholder="/dev/ttyUSB0" class="input" style="flex:1">
          <input v-model.number="baudrate" type="number" placeholder="115200" class="input" style="width:100px">
          <button @click="connectSerial" class="btn btn-primary" :disabled="!connected || !!status.connected">连接</button>
          <button @click="disconnectSerial" class="btn btn-danger" :disabled="!connected || !status.connected">断开</button>
        </div>
        <div v-if="status.serial_device" class="hint">当前设备: {{ status.serial_device }}</div>
        <div v-if="result" class="result" :class="result.status === 'ok' ? 'ok' : 'err'">
          {{ JSON.stringify(result) }}
        </div>
      </div>
    </div>

    <!-- 网关信息 -->
    <div class="card">
      <div class="card-header">网关节点</div>
      <div class="card-body">
        <table class="info-table">
          <tr><td>节点 ID</td><td class="mono">{{ status.my_node_id || '—' }}</td></tr>
          <tr><td>串口状态</td><td>
            <span class="badge" :class="status.connected ? 'badge-green' : 'badge-red'">
              {{ status.connected ? '已连接' : '未连接' }}
            </span>
          </td></tr>
        </table>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useGateway } from '../composables/useGateway.js'

const { connected, status, sendCmd } = useGateway()
const device = ref('/dev/ttyUSB0')
const baudrate = ref(115200)
const result = ref(null)

async function connectSerial() {
  result.value = await sendCmd({ cmd: 'connect_serial', device: device.value, baudrate: baudrate.value })
  if (result.value?.status === 'ok') sendCmd({ cmd: 'get_status' })
}
async function disconnectSerial() {
  result.value = await sendCmd({ cmd: 'disconnect_serial' })
  if (result.value?.status === 'ok') sendCmd({ cmd: 'get_status' })
}

function formatUptime(s) {
  if (!s) return '—'
  if (s < 60) return s + 's'
  if (s < 3600) return Math.floor(s / 60) + 'm' + (s % 60) + 's'
  return Math.floor(s / 3600) + 'h' + Math.floor((s % 3600) / 60) + 'm'
}
</script>
