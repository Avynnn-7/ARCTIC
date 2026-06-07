/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        finance: {
          bg: '#02040a',
          panel: '#0a0d14',
          border: '#1a1f2c',
          text: '#e2e8f0',
          muted: '#64748b',
          accent: '#0ea5e9', // Deep sky blue / teal
          accent_hover: '#38bdf8',
          success: '#10b981',
          danger: '#ef4444',
          gold: '#fbbf24',
        }
      },
      fontFamily: {
        sans: ['Inter', 'sans-serif'],
        mono: ['JetBrains Mono', 'monospace'],
      },
      animation: {
        'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
      }
    },
  },
  plugins: [],
}
