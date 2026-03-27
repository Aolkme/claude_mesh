<template>
  <div class="app">
    <!-- 顶部导航 -->
    <header class="app-header">
      <span class="brand">📡 MeshGateway</span>
      <div class="badges">
        <span class="badge" :class="connected ? 'badge-green' : 'badge-red'">
          WS: {{ connected ? '已连接' : '断开' }}
        </span>
        <span class="badge" :class="status.connected ? 'badge-green' : 'badge-red'">
          串口: {{ status.connected ? '已连接' : '未连接' }}
        </span>
        <span v-if="status.my_node_id" class="node-id">{{ status.my_node_id }}</span>
      </div>
      <nav class="nav">
        <button
          v-for="tab in tabs"
          :key="tab.id"
          @click="activeTab = tab.id"
          class="nav-btn"
          :class="{ active: activeTab === tab.id }"
        >{{ tab.label }}</button>
      </nav>
    </header>

    <!-- 内容区 -->
    <main class="app-main">
      <Dashboard v-if="activeTab === 'dashboard'" />
      <Nodes      v-else-if="activeTab === 'nodes'" />
      <Messages   v-else-if="activeTab === 'messages'" />
      <Monitor    v-else-if="activeTab === 'monitor'" />
    </main>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useGateway } from './composables/useGateway.js'
import Dashboard from './views/Dashboard.vue'
import Nodes     from './views/Nodes.vue'
import Messages  from './views/Messages.vue'
import Monitor   from './views/Monitor.vue'

const { connected, status } = useGateway()
const activeTab = ref('dashboard')
const tabs = [
  { id: 'dashboard', label: '状态' },
  { id: 'nodes',     label: '节点' },
  { id: 'messages',  label: '消息' },
  { id: 'monitor',   label: '监控' },
]
</script>
