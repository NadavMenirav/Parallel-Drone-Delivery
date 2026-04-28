import axios from 'axios';

const api = axios.create({
  baseURL: '/api',
  headers: { 'Content-Type': 'application/json' },
});

export const simAPI = {
  getState: () => api.get('/state'),
  runMock: (mock = 'mock1') => api.post(`/run-mock/${mock}`),
};
