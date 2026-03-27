<template>
  <div>
    <div class="toolbar" style="justify-content:space-between">
      <span class="hint">{{ events.length }} 条事件（最多 200 条）</span>
      <button @click="clearLocal" class="btn btn-secondary">清空显示</button>
    </div>
    <div class="card" style="margin-top:12px">
      <div class="log-area" ref="logRef">
        <div v-if="!localEvents.length" class="empty">等待事件...</div>
        <div
          v-for="(ev, i) in localEvents"
          :key="i"
          class="log-row"
          :class="'log-' + ev.type"
        >
          <span class="log-ts">{{ ev.ts }}</span>
          <span class="log-body">{{ formatEvent(ev.data) }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch, nextTick } from 'vue'
import { useGateway } from '../composables/useGateway.js'

const { events } = useGateway()
const logRef = ref(null)
const localEvents = ref([...events.value])

// 同步新事件
watch(events, (newEvents) => {
  localEvents.value = [...newEvents]
  nextTick(() => {
    if (logRef.value) {
      // 只有接近底部时才自动滚动
      const el = logRef.value
      const nearBottom = el.scrollTop + el.clientHeight >= el.scrollHeight - 60
      if (nearBottom) el.scrollTop = el.scrollHeight
    }
  })
}, { deep: true })

function clearLocal() { localEvents.value = [] }

function formatEvent(d) {
  if (typeof d === 'string') return d
  return JSON.stringify(d)
}
</script>
