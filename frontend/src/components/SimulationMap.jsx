const PAD = 55;

function pointOf(item) {
  return {
    x: Number(item?.pos?.x ?? item?.x ?? 0),
    y: Number(item?.pos?.y ?? item?.y ?? 0),
  };
}

function normalizeStatus(status = '') {
  return String(status).replace('CUSTOMER_', '').toLowerCase();
}

function droneStatus(drone) {
  if (
    Number(drone.targetCustomerId) > 0 ||
    drone.currentCustomer ||
    drone.currentCustomerId ||
    String(drone.status || '').toLowerCase().includes('deliver')
  ) {
    return 'delivering';
  }

  return 'idle';
}

export default function SimulationMap({
  bakeries = [],
  customers = [],
  drones = [],
  selectedMock = 'mock1',
}) {
  const allPoints = [...bakeries, ...customers, ...drones].map(pointOf);

  const minX = Math.min(...allPoints.map((p) => p.x), 0);
  const maxX = Math.max(...allPoints.map((p) => p.x), 100);
  const minY = Math.min(...allPoints.map((p) => p.y), 0);
  const maxY = Math.max(...allPoints.map((p) => p.y), 100);

  const width = 1000;
  const height = 620;

  const scaleX = (x) =>
    PAD + ((x - minX) / Math.max(1, maxX - minX)) * (width - PAD * 2);

  const scaleY = (y) =>
    PAD + ((y - minY) / Math.max(1, maxY - minY)) * (height - PAD * 2);

  const scaled = (item) => {
    const p = pointOf(item);
    return { x: scaleX(p.x), y: scaleY(p.y) };
  };

  return (
    <div className="map-shell">
      <div className="map-mode-label">
        {selectedMock === 'mock1' && 'Mock 1: Balanced delivery'}
        {selectedMock === 'mock2' && 'Mock 2: Split order'}
        {selectedMock === 'mock3' && 'Mock 3: Multi-stop route'}
      </div>

      <svg
        viewBox={`0 0 ${width} ${height}`}
        className="map-svg"
        role="img"
        aria-label="Simulation map"
      >
        <defs>
          <pattern id="grid" width="50" height="50" patternUnits="userSpaceOnUse">
            <path
              d="M 50 0 L 0 0 0 50"
              fill="none"
              stroke="#dbe3ef"
              strokeWidth="1"
            />
          </pattern>

          <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
            <feDropShadow dx="0" dy="8" stdDeviation="8" floodOpacity="0.16" />
          </filter>

          <marker
            id="arrow"
            markerWidth="12"
            markerHeight="12"
            refX="9"
            refY="3"
            orient="auto"
            markerUnits="strokeWidth"
          >
            <path d="M0,0 L0,6 L10,3 z" fill="#6366f1" />
          </marker>
        </defs>

        <rect width={width} height={height} rx="26" fill="#f8fafc" />
        <rect
          x="18"
          y="18"
          width={width - 36}
          height={height - 36}
          rx="22"
          fill="url(#grid)"
          stroke="#d7e0ec"
        />

        {drones.map((d) => {
          const start = scaled(d);

          const targetCustomer = customers.find(
            (c) =>
              Number(c.id) === Number(d.currentCustomerId) ||
              Number(c.id) === Number(d.customerId) ||
              Number(c.id) === Number(d.targetCustomerId)
          );

          if (!targetCustomer || droneStatus(d) === 'idle') return null;

          const end = scaled(targetCustomer);

          return (
            <line
              key={`route-${d.id}`}
              x1={start.x}
              y1={start.y}
              x2={end.x}
              y2={end.y}
              stroke="#6366f1"
              strokeWidth="4"
              strokeDasharray="10 9"
              markerEnd="url(#arrow)"
              opacity="0.75"
            />
          );
        })}

        {bakeries.map((b) => {
          const p = scaled(b);

          return (
            <g key={`bakery-${b.id}`} filter="url(#shadow)">
              <circle
                cx={p.x}
                cy={p.y}
                r="31"
                fill="#fff7ed"
                stroke="#f97316"
                strokeWidth="4"
              />
              <text x={p.x} y={p.y + 8} textAnchor="middle" fontSize="26">
                🥖
              </text>
              <text
                x={p.x}
                y={p.y - 40}
                textAnchor="middle"
                fontSize="16"
                fontWeight="800"
                fill="#9a3412"
              >
                B{b.id}
              </text>
              <text
                x={p.x}
                y={p.y + 51}
                textAnchor="middle"
                fontSize="13"
                fontWeight="700"
                fill="#9a3412"
              >
                {b.inventory}/{b.capacity}
              </text>
              <title>{`Bakery ${b.id}: ${b.inventory}/${b.capacity}`}</title>
            </g>
          );
        })}

        {customers.map((c) => {
          const p = scaled(c);
          const status = normalizeStatus(c.status);

          const active = status === 'active';
          const departed = status === 'departed';
          const served = status === 'served' || departed;

          return (
            <g key={`customer-${c.id}`} filter="url(#shadow)">
              <circle
                cx={p.x}
                cy={p.y}
                r="24"
                fill={served ? '#dcfce7' : active ? '#dbeafe' : '#f1f5f9'}
                stroke={served ? '#16a34a' : active ? '#2563eb' : '#94a3b8'}
                strokeWidth="4"
              />

              <text
                x={p.x}
                y={p.y + 6}
                textAnchor="middle"
                fontSize="15"
                fontWeight="900"
                fill={served ? '#166534' : active ? '#1d4ed8' : '#475569'}
              >
                C{c.id}
              </text>

              <text
                x={p.x}
                y={p.y + 43}
                textAnchor="middle"
                fontSize="12"
                fontWeight="700"
                fill={served ? '#166534' : '#334155'}
              >
                {departed ? 'served + left' : `d:${c.demand} p:${c.priority}`}
              </text>

              <title>
                {`Customer ${c.id}: ${departed ? 'served + left' : status}, demand ${c.demand}, priority ${c.priority}`}
              </title>
            </g>
          );
        })}

        {drones.map((d) => {
          const current = scaled(d);
          const busy = droneStatus(d) === 'delivering';

          return (
            <g
              key={`drone-${d.id}`}
              className="drone-position"
              style={{
                transform: `translate(${current.x}px, ${current.y}px)`,
                transition: 'transform 0.8s ease-in-out',
              }}
            >
              <g className="drone-node" filter="url(#shadow)">
                <circle
                  cx="0"
                  cy="0"
                  r="28"
                  fill={busy ? '#ede9fe' : '#ffffff'}
                  stroke={busy ? '#7c3aed' : '#334155'}
                  strokeWidth="4"
                />

                <text x="0" y="8" textAnchor="middle" fontSize="25">
                  🚁
                </text>

                <text
                  x="0"
                  y="-39"
                  textAnchor="middle"
                  fontSize="15"
                  fontWeight="900"
                  fill="#111827"
                >
                  D{d.id}
                </text>

                <text
                  x="0"
                  y="51"
                  textAnchor="middle"
                  fontSize="12"
                  fontWeight="800"
                  fill="#111827"
                >
                  {d.load ?? 0}/{d.capacity ?? '-'}
                </text>

                <title>
                  {`Drone ${d.id}: ${busy ? 'delivering' : 'idle'}, load ${d.load ?? 0}/${d.capacity ?? '-'}`}
                </title>
              </g>
            </g>
          );
        })}
      </svg>
    </div>
  );
}