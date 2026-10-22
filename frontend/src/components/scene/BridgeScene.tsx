import { useRef } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, OrthographicCamera, Text } from '@react-three/drei';
import * as THREE from 'three';
import {
  BARRIER_Z,
  BRIDGE_HALF_LENGTH,
  BRIDGE_LENGTH,
  BRIDGE_WIDTH,
  ENTRY_ROAD_CENTER_Z,
  ENTRY_ROAD_LENGTH,
  ENTRY_Z,
  EXIT_ROAD_CENTER_Z,
  EXIT_ROAD_LENGTH,
  EXIT_Z,
  LANE_CENTERS,
  LANE_WIDTH,
  MARKING_Y,
  QUEUE_GAP,
  RIVER_WIDTH,
  ROAD_TOP_Y,
  ROAD_WIDTH,
  WAITING_LINE_Z,
  WORLD_WIDTH,
  validateSceneConfig,
} from '../../sceneConfig';

export type SceneVehicle = {
  id: number;
  vehicleType: string;
  state: string;
};

type Vec3 = [number, number, number];

const configErrors = validateSceneConfig();
if (configErrors.length) throw new Error(configErrors.join('\n'));

function Block({ position, size, color }: { position: Vec3; size: Vec3; color: string }) {
  return (
    <mesh position={position} castShadow receiveShadow>
      <boxGeometry args={size} />
      <meshStandardMaterial color={color} flatShading />
    </mesh>
  );
}

function Terrain() {
  const bankLength = ENTRY_Z - RIVER_WIDTH / 2 + 3;
  const northCenter = (ENTRY_Z + RIVER_WIDTH / 2 + 3) / 2;
  const southLength = Math.abs(EXIT_Z) - RIVER_WIDTH / 2 + 3;
  const southCenter = (EXIT_Z - RIVER_WIDTH / 2 - 3) / 2;
  return (
    <group>
      <Block position={[0, -0.28, northCenter]} size={[WORLD_WIDTH, 0.58, bankLength]} color="#67cc3e" />
      <Block position={[0, -0.28, southCenter]} size={[WORLD_WIDTH, 0.58, southLength]} color="#67cc3e" />
    </group>
  );
}

function River() {
  const reflections = [-8, -5.2, -2.3, 1, 4.1, 7.2];
  const ripples = useRef<THREE.Group>(null);
  useFrame((_, delta) => {
    if (!ripples.current) return;
    ripples.current.position.x += delta * 0.18;
    if (ripples.current.position.x > 1.4) ripples.current.position.x = 0;
  });
  return (
    <group>
      <Block position={[0, -0.42, 0]} size={[WORLD_WIDTH, 0.22, RIVER_WIDTH]} color="#159fe5" />
      <group ref={ripples}>
        {reflections.map((x, index) => (
          <Block key={x} position={[x, -0.295, index % 2 ? -1.25 : 1.15]} size={[0.85, 0.025, 0.12]} color="#74d9f4" />
        ))}
      </group>
    </group>
  );
}

function RoadMarkings() {
  const dashSpacing = 1.45;
  const dashLength = 0.72;
  const dashes: number[] = [];
  for (let z = EXIT_Z; z <= ENTRY_Z; z += dashSpacing) dashes.push(z);
  return (
    <group>
      {dashes.map(z => <Block key={z} position={[0, MARKING_Y, z]} size={[0.11, 0.025, dashLength]} color="#fffdf2" />)}
      {[-ROAD_WIDTH / 2 + 0.09, ROAD_WIDTH / 2 - 0.09].map(x => (
        <Block key={x} position={[x, MARKING_Y, (ENTRY_Z + EXIT_Z) / 2]} size={[0.12, 0.025, ENTRY_Z - EXIT_Z]} color="#fffdf2" />
      ))}
    </group>
  );
}

function Road() {
  const roadThickness = 0.24;
  return (
    <group>
      <Block position={[0, ROAD_TOP_Y - roadThickness / 2, ENTRY_ROAD_CENTER_Z]} size={[ROAD_WIDTH, roadThickness, ENTRY_ROAD_LENGTH]} color="#444851" />
      <Block position={[0, ROAD_TOP_Y - roadThickness / 2, EXIT_ROAD_CENTER_Z]} size={[ROAD_WIDTH, roadThickness, EXIT_ROAD_LENGTH]} color="#444851" />
      <RoadMarkings />
    </group>
  );
}

function Bridge() {
  const plankCount = 14;
  const plankDepth = BRIDGE_LENGTH / plankCount;
  const posts = [-BRIDGE_HALF_LENGTH + 0.35, -0.95, 0.95, BRIDGE_HALF_LENGTH - 0.35];
  return (
    <group>
      <Block position={[0, ROAD_TOP_Y - 0.16, 0]} size={[BRIDGE_WIDTH, 0.32, BRIDGE_LENGTH]} color="#955e32" />
      {Array.from({ length: plankCount }, (_, index) => {
        const z = -BRIDGE_HALF_LENGTH + plankDepth / 2 + index * plankDepth;
        return <Block key={z} position={[0, ROAD_TOP_Y + 0.02, z]} size={[BRIDGE_WIDTH, 0.04, plankDepth - 0.035]} color={index % 2 ? '#c98c50' : '#d49a59'} />;
      })}
      {[-BRIDGE_WIDTH / 2 - 0.12, BRIDGE_WIDTH / 2 + 0.12].map(x => (
        <group key={x}>
          {posts.map(z => <Block key={z} position={[x, ROAD_TOP_Y + 0.62, z]} size={[0.18, 1.25, 0.18]} color="#67513b" />)}
          <Block position={[x, ROAD_TOP_Y + 1.05, 0]} size={[0.16, 0.14, BRIDGE_LENGTH]} color="#e5c58e" />
          <Block position={[x, ROAD_TOP_Y + 0.62, 0]} size={[0.12, 0.12, BRIDGE_LENGTH]} color="#f0d6a3" />
        </group>
      ))}
    </group>
  );
}

function Barrier({ open }: { open: boolean }) {
  const pivotX = -ROAD_WIDTH / 2 - 0.28;
  return (
    <group position={[0, 0, BARRIER_Z]}>
      <Block position={[pivotX, ROAD_TOP_Y + 0.55, 0]} size={[0.44, 1.1, 0.5]} color="#f39a18" />
      <Block position={[-pivotX, ROAD_TOP_Y + 0.55, 0]} size={[0.44, 1.1, 0.5]} color="#f39a18" />
      <group position={[pivotX, ROAD_TOP_Y + 0.88, 0]} rotation={[0, 0, open ? -Math.PI / 2 : 0]}>
        <Block position={[ROAD_WIDTH / 2 + 0.28, 0, 0]} size={[ROAD_WIDTH + 0.56, 0.16, 0.18]} color="#fff7e7" />
        {Array.from({ length: 7 }, (_, index) => (
          <Block key={index} position={[0.55 + index * 0.76, 0, 0.1]} size={[0.34, 0.19, 0.04]} color="#df3434" />
        ))}
      </group>
    </group>
  );
}

function WeightSign({ weight }: { weight: number }) {
  const color = weight > 15 ? '#ff544d' : weight === 15 ? '#ffb237' : '#69ef62';
  const signZ = BARRIER_Z + 0.75;
  return (
    <group position={[0, 0, signZ]}>
      {[-ROAD_WIDTH / 2 - 0.38, ROAD_WIDTH / 2 + 0.38].map(x => <Block key={x} position={[x, 1.55, 0]} size={[0.16, 2.45, 0.18]} color="#525b61" />)}
      <Block position={[0, 2.25, 0]} size={[ROAD_WIDTH + 0.42, 0.72, 0.18]} color="#181c21" />
      <Text position={[0, 2.25, 0.1]} fontSize={0.28} color={color} anchorX="center" anchorY="middle">POIDS ACTUEL : {weight} / 15 T</Text>
    </group>
  );
}

function Tree({ position, scale = 1 }: { position: Vec3; scale?: number }) {
  return (
    <group position={position} scale={scale}>
      <Block position={[0, 0.42, 0]} size={[0.34, 0.92, 0.34]} color="#744422" />
      <Block position={[0, 1.02, 0]} size={[1.28, 0.46, 1.28]} color="#168c31" />
      <Block position={[0, 1.39, 0]} size={[0.92, 0.42, 0.92]} color="#24b63a" />
      <Block position={[0, 1.72, 0]} size={[0.58, 0.34, 0.58]} color="#55d843" />
    </group>
  );
}

function Scenery() {
  const trees: Array<[Vec3, number]> = [
    [[-7.6, 0, 8.8], 1.08], [[-5.3, 0, 5.7], 0.82], [[6.5, 0, 9.5], 1],
    [[7.7, 0, 5.2], 0.78], [[-7.2, 0, -7.5], 1.05], [[-5.1, 0, -10], 0.78],
    [[6.7, 0, -8.2], 1.12], [[8.1, 0, -5.4], 0.82],
  ];
  return <group>{trees.map(([position, scale], i) => <Tree key={i} position={position} scale={scale} />)}</group>;
}

function VehicleModel({ truck, color }: { truck: boolean; color: string }) {
  const bodyWidth = truck ? 1.68 : 1.38;
  const bodyLength = truck ? 2.2 : 1.75;
  return (
    <group>
      <Block position={[0, 0.33, 0]} size={[bodyWidth, 0.48, bodyLength]} color={color} />
      {truck ? (
        <><Block position={[0, 0.72, 0.35]} size={[bodyWidth - 0.12, 0.72, 1.34]} color="#edf2f5" /><Block position={[0, 0.57, -0.82]} size={[bodyWidth - 0.15, 0.5, 0.48]} color={color} /></>
      ) : (
        <Block position={[0, 0.68, -0.08]} size={[bodyWidth - 0.28, 0.36, 0.84]} color="#9ed4e8" />
      )}
      {[-bodyWidth / 2, bodyWidth / 2].flatMap(x => [-bodyLength * 0.3, bodyLength * 0.3].map(z => (
        <mesh key={`${x}-${z}`} position={[x, 0.15, z]} rotation={[0, 0, Math.PI / 2]} castShadow>
          <cylinderGeometry args={[0.22, 0.22, 0.15, 10]} /><meshStandardMaterial color="#202226" />
        </mesh>
      )))}
    </group>
  );
}

function Vehicle3D({ vehicle, queueIndex }: { vehicle: SceneVehicle; queueIndex: number }) {
  const group = useRef<THREE.Group>(null);
  const lane = LANE_CENTERS[vehicle.id % 2];
  const truck = vehicle.vehicleType === 'TRUCK';
  const waitingZ = WAITING_LINE_Z + queueIndex * QUEUE_GAP;
  const initialZ = Math.min(ENTRY_Z - 0.9, waitingZ + 2.8);
  const positionZ = useRef(initialZ);
  const crossing = vehicle.state === 'CROSSING' || vehicle.state === 'EXITING';
  useFrame((_, delta) => {
    if (!group.current) return;
    const targetZ = crossing ? EXIT_Z - 1 : waitingZ;
    const speed = crossing ? 3.1 : 2.2;
    positionZ.current = Math.max(targetZ, positionZ.current - delta * speed);
    group.current.position.set(lane, ROAD_TOP_Y, positionZ.current);
  });
  const colors = ['#f3c529', '#eb493e', '#2c8aed', '#22ae4d'];
  return <group ref={group} position={[lane, ROAD_TOP_Y, initialZ]}><VehicleModel truck={truck} color={truck ? '#2784df' : colors[vehicle.id % colors.length]} /></group>;
}

export function BridgeScene({ vehicles, weight }: { vehicles: SceneVehicle[]; weight: number }) {
  return (
    <Canvas shadows>
      <OrthographicCamera
        makeDefault
        position={[13, 16, 19]}
        zoom={42}
        near={0.1}
        far={100}
        onUpdate={camera => camera.lookAt(0, 0, 0.7)}
      />
      <color attach="background" args={['#85dc4b']} />
      <ambientLight intensity={1.65} />
      <directionalLight castShadow position={[-7, 15, 11]} intensity={2.2} shadow-mapSize={[2048, 2048]} />
      <Terrain /><River /><Road /><Bridge /><Barrier open={weight > 0} /><WeightSign weight={weight} /><Scenery />
      {vehicles.map((vehicle, index) => <Vehicle3D key={vehicle.id} vehicle={vehicle} queueIndex={Math.floor(index / 2)} />)}
      <OrbitControls enablePan={false} minZoom={34} maxZoom={56} minPolarAngle={0.65} maxPolarAngle={1.05} target={[0, 0, 0.7]} />
    </Canvas>
  );
}
