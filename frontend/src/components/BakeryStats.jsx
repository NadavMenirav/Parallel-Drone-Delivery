function getPercent(value, max) {
  if (!max) return 0;
  return Math.max(0, Math.min(100, Math.round((Number(value || 0) / Number(max)) * 100)));
}

export default function BakeryStats({ bakeries = [] }) {
  if (!bakeries.length) return <div className="empty-state">No bakeries initialized.</div>;

  return (
    <div className="card-list">
      {bakeries.map((b) => {
        const percent = getPercent(b.inventory, b.capacity);
        const low = percent <= 20;
        return (
          <div className="bakery-card" key={b.id}>
            <div className="card-row">
              <div>
                <strong>Bakery {b.id}</strong>
                <small>Position ({Math.round(b.pos?.x ?? b.x ?? 0)}, {Math.round(b.pos?.y ?? b.y ?? 0)})</small>
              </div>
              <span className={low ? 'pill red' : 'pill green'}>{b.inventory}/{b.capacity}</span>
            </div>
            <div className="progress-track">
              <div className={low ? 'progress-fill red' : 'progress-fill orange'} style={{ width: `${percent}%` }} />
            </div>
          </div>
        );
      })}
    </div>
  );
}
