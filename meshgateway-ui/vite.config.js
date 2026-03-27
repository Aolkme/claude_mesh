import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve } from 'path'

// https://vite.dev/config/
export default defineConfig({
  plugins: [vue()],

  // 构建产物直接输出到 meshgateway/static/，由 Mongoose 服务
  build: {
    outDir: resolve(__dirname, '../meshgateway/static'),
    emptyOutDir: true,
  },

  // 开发模式代理：API 和 WebSocket 请求转发到 meshgateway 守护进程
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:8080',
        changeOrigin: true,
      },
      '/ws': {
        target: 'ws://127.0.0.1:8080',
        ws: true,
        changeOrigin: true,
      },
    },
  },
})
