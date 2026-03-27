<template>
  <div>
    <div class="toolbar">
      <button @click="refresh" class="btn btn-secondary">刷新</button>
      <span class="hint">共 {{ nodes.length }} 个节点</span>
    </div>
    <div class="card" style="margin-top:12px;padding:0;overflow-x:auto">
      <table class="data-table">
        <thead>
          <tr>
            <th>节点 ID</th>
            <th>长名称</th>
            <th>短名称</th>
            <th>电量%</th>
            <th>电压V</th>
            <th>SNR</th>
            <th>RSSI</th>
            <th>跳数</th>
            <th>纬度</th>
            <th>经度</th>
            <th>已入网</th>
            <th>最后联系</th>
          </tr>
        </thead>
        <tbody>
          <tr v-if="!nodes.length">
            <td colspan="12" class="empty">暂无节点，请先连接串口并等待节点信息推送</td>
          </tr>
          <tr v-for="n in nodes" :key="n.node_id">
            <td class="mono text-blue">{{ n.node_id }}</td>
            <td>{{ n.long_name || '—' }}</td>
            <td class="mono">{{ n.short_name || '—' }}</td>
            <td>{{ n.battery }}%</td>
            <td>{{ n.voltage?.toFixed(2) }}</td>
            <td>{{ n.snr?.toFixed(1) }}</td>
            <td>{{ n.rssi }}</td>
            <td>{{ n.hops_away }}</td>
            <td>{{ n.latitude ? n.latitude.toFixed(5) : '—' }}</td>
            <td>{{ n.longitude ? n.longitude.toFixed(5) : '—' }}</td>
            <td>{{ n.is_enrolled ? '✓' : '' }}</td>
            <td class="hint">{{ formatTime(n.last_heard) }}</td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>

<script setup>
import { useGateway } from '../composables/useGateway.js'

const { nodes, sendCmd } = useGateway()

function refresh() { sendCmd({ cmd: 'get_nodes' }) }

function formatTime(ms) {
  if (!ms) return '—'
  return new Date(ms).toLocaleTimeString()
}
</script>
