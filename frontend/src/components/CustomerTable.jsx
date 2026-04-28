function normalizeStatus(status = '') {
  return String(status).replace('CUSTOMER_', '').toLowerCase();
}

export default function CustomerTable({ customers = [] }) {
  if (!customers.length) return <div className="empty-state">No customers received from simulation.</div>;

  const sorted = [...customers].sort((a, b) => {
    const statusA = normalizeStatus(a.status) === 'active' ? 0 : 1;
    const statusB = normalizeStatus(b.status) === 'active' ? 0 : 1;
    if (statusA !== statusB) return statusA - statusB;
    return Number(b.priority || 0) - Number(a.priority || 0);
  });

  return (
    <div className="table-wrap">
      <table className="data-table">
        <thead>
          <tr>
            <th>Customer</th>
            <th>Demand</th>
            <th>Priority</th>
            <th>Status</th>
          </tr>
        </thead>
        <tbody>
          {sorted.map((c) => {
            const status = normalizeStatus(c.status);
            return (
              <tr key={c.id}>
                <td><strong>C{c.id}</strong></td>
                <td>{c.demand}</td>
                <td>{c.priority}</td>
                <td><span className={`status-pill ${status}`}>{status || 'unknown'}</span></td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}
