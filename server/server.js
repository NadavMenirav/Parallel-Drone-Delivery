import express from 'express';
import cors from 'cors';
import { execFile } from 'child_process';
import fs from 'fs';
import path from 'path';

const app = express();

app.use(cors());
app.use(express.json());

const PROJECT_ROOT = path.resolve('..');
const SIM_EXEC = path.join(PROJECT_ROOT, 'simulation');
const STATE_FILE = path.join(PROJECT_ROOT, 'state.json');

function readState(res) {
  if (!fs.existsSync(STATE_FILE)) {
    return res.status(404).json({ error: 'state.json not found' });
  }

  const data = fs.readFileSync(STATE_FILE, 'utf-8');
  return res.json(JSON.parse(data));
}

app.get('/api/state', (req, res) => {
  return readState(res);
});

app.post('/api/run-mock/:mock', (req, res) => {
  const mock = req.params.mock || 'mock1';

  console.log(`Running simulation with ${mock}`);
  console.log(`Project root: ${PROJECT_ROOT}`);
  console.log(`Simulation path: ${SIM_EXEC}`);
  console.log(`State file: ${STATE_FILE}`);

  execFile(SIM_EXEC, ['--export', mock], { cwd: PROJECT_ROOT }, (error, stdout, stderr) => {
    if (error) {
      console.error('Simulation error:', error);
      console.error('stderr:', stderr);
      return res.status(500).json({ error: 'Simulation crashed' });
    }

    return readState(res);
  });
});

app.listen(3000, () => {
  console.log('API Bridge Server running on http://localhost:3000');
});