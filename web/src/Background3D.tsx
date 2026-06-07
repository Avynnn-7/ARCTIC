import { useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import { useScroll } from '@react-three/drei';
import * as THREE from 'three';

export function Background3D() {
  const scroll = useScroll();
  const particlesRef = useRef<THREE.Points>(null);
  
  // Generate random particles for a subtle data-flow background
  const particleCount = 2000;
  const positions = new Float32Array(particleCount * 3);
  const opacities = new Float32Array(particleCount);
  
  for (let i = 0; i < particleCount; i++) {
    positions[i * 3] = (Math.random() - 0.5) * 20; // x
    positions[i * 3 + 1] = (Math.random() - 0.5) * 20; // y
    positions[i * 3 + 2] = (Math.random() - 0.5) * 10; // z
    opacities[i] = Math.random();
  }

  useFrame((state) => {
    if (!particlesRef.current) return;
    const t = state.clock.getElapsedTime();
    const offset = scroll.offset; // 0 to 1

    // Rotate slowly based on time and scroll offset
    particlesRef.current.rotation.y = t * 0.05 + offset * Math.PI * 0.5;
    particlesRef.current.rotation.x = offset * 0.5;
    
    // Slight pulse effect
    particlesRef.current.position.y = Math.sin(t * 0.5) * 0.2;
  });

  return (
    <points ref={particlesRef}>
      <bufferGeometry>
        <bufferAttribute
          attach="attributes-position"
          args={[positions, 3]}
        />
        <bufferAttribute
          attach="attributes-opacity"
          args={[opacities, 1]}
        />
      </bufferGeometry>
      <pointsMaterial
        size={0.02}
        color="#0ea5e9"
        transparent
        opacity={0.3}
        blending={THREE.AdditiveBlending}
      />
    </points>
  );
}
