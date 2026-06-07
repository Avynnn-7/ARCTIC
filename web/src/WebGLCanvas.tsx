import { useEffect, useRef } from 'react';

interface WebGLCanvasProps {
  wasmEngine: any;
  wasmMemory: any;
  numPoints: number;
}

export const WebGLCanvas: React.FC<WebGLCanvasProps> = ({ wasmEngine, wasmMemory, numPoints }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    
    const gl = canvas.getContext('webgl', { alpha: false, antialias: true });
    if (!gl) return;

    const resizeCanvas = () => {
      const rect = canvas.parentElement?.getBoundingClientRect();
      if (rect) {
        const dpr = window.devicePixelRatio || 1;
        canvas.width = rect.width * dpr;
        canvas.height = rect.height * dpr;
        canvas.style.width = rect.width + 'px';
        canvas.style.height = rect.height + 'px';
        gl.viewport(0, 0, canvas.width, canvas.height);
      }
    };
    resizeCanvas();
    
    const resizeObserver = new ResizeObserver(resizeCanvas);
    if (canvas.parentElement) {
      resizeObserver.observe(canvas.parentElement);
    }

    const vsSource = `
      attribute vec2 aVertexPosition;
      void main() {
        gl_Position = vec4(aVertexPosition, 0.0, 1.0);
      }
    `;
    const fsSource = `
      precision mediump float;
      uniform vec4 uColor;
      void main() {
        gl_FragColor = uColor;
      }
    `;

    const loadShader = (type: number, source: string) => {
      const shader = gl.createShader(type)!;
      gl.shaderSource(shader, source);
      gl.compileShader(shader);
      return shader;
    };

    const shaderProgram = gl.createProgram()!;
    gl.attachShader(shaderProgram, loadShader(gl.VERTEX_SHADER, vsSource));
    gl.attachShader(shaderProgram, loadShader(gl.FRAGMENT_SHADER, fsSource));
    gl.linkProgram(shaderProgram);
    gl.useProgram(shaderProgram);

    const posLoc = gl.getAttribLocation(shaderProgram, "aVertexPosition");
    gl.enableVertexAttribArray(posLoc);
    const colorLoc = gl.getUniformLocation(shaderProgram, "uColor");

    const buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.vertexAttribPointer(posLoc, 2, gl.FLOAT, false, 0, 0);

    const vertices = new Float32Array(numPoints * 2);
    for (let i = 0; i < numPoints; ++i) {
      vertices[i * 2] = (i / numPoints) * 2.0 - 1.0; 
    }

    const drawGrid = () => {
      const yScale = 5.0;
      for (let val = -4; val <= 4; val++) {
        const yNdc = val / yScale;
        const gridLine = new Float32Array([-1.0, yNdc, 1.0, yNdc]);
        gl.bufferData(gl.ARRAY_BUFFER, gridLine, gl.STATIC_DRAW);
        if (val === 0) {
          gl.uniform4fv(colorLoc, [0.18, 0.20, 0.26, 1.0]);
        } else {
          gl.uniform4fv(colorLoc, [0.09, 0.10, 0.13, 1.0]);
        }
        gl.drawArrays(gl.LINES, 0, 2);
      }
      for (let i = 0; i <= numPoints; i += 200) {
        const xNdc = (i / numPoints) * 2.0 - 1.0;
        const gridLine = new Float32Array([xNdc, -1.0, xNdc, 1.0]);
        gl.bufferData(gl.ARRAY_BUFFER, gridLine, gl.STATIC_DRAW);
        gl.uniform4fv(colorLoc, [0.07, 0.08, 0.10, 1.0]);
        gl.drawArrays(gl.LINES, 0, 2);
      }
    };

    let animationId: number;

    const renderLoop = () => {
      if (!wasmEngine) return;
      
      wasmEngine.step_frame(10);
      const head = wasmEngine.get_head();

      const ouPtr = wasmEngine.get_ou_signal_ptr() / 4; 
      const bAPtr = wasmEngine.get_boundary_a_ptr() / 4;
      const bBPtr = wasmEngine.get_boundary_b_ptr() / 4;
      
      const heapF32 = new Float32Array(wasmMemory.buffer);

      // Match the dark background: rgb(6, 8, 12) = #06080c
      gl.clearColor(0.024, 0.031, 0.047, 1.0);
      gl.clear(gl.COLOR_BUFFER_BIT);
      
      gl.enable(gl.BLEND);
      gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

      drawGrid();

      const drawLineStrip = (dataPtr: number, color: number[]) => {
        for (let i = 0; i < numPoints; ++i) {
          const bufferIdx = (head + i) % numPoints;
          vertices[i * 2 + 1] = heapF32[dataPtr + bufferIdx] / 5.0; 
        }
        
        gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.DYNAMIC_DRAW);
        gl.uniform4fv(colorLoc, color);
        gl.drawArrays(gl.LINE_STRIP, 0, numPoints);
      };

      // Competitor boundary: muted purple
      drawLineStrip(bBPtr, [0.66, 0.33, 0.97, 0.5]);
      // Our adaptive boundary: cyan
      drawLineStrip(bAPtr, [0.02, 0.71, 0.83, 0.7]);
      // OU signal: clean white with a hint of blue
      drawLineStrip(ouPtr, [0.82, 0.85, 0.92, 0.9]);

      animationId = requestAnimationFrame(renderLoop);
    };

    renderLoop();

    return () => {
      cancelAnimationFrame(animationId);
      resizeObserver.disconnect();
      gl.deleteBuffer(buffer);
      gl.deleteProgram(shaderProgram);
    };
  }, [wasmEngine, numPoints, wasmMemory]);

  return (
    <canvas ref={canvasRef} className="block w-full h-full" />
  );
};
