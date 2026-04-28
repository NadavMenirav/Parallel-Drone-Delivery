export default function ControlPanel({
  onRefresh,
  onGenerate,
  onNextRound,
  onPrevRound,
  isPlaying,
  togglePlay,
  isLoading,
  currentFrame,
  totalFrames,
  onJump,
  playSpeed,
  setPlaySpeed,
}) {
  const lastFrame = Math.max(totalFrames - 1, 0);

  return (
    <div className="control-panel">
      <div className="control-actions">
        <button className="btn ghost" onClick={onRefresh} disabled={isLoading}>Refresh</button>
        <button className="btn primary" onClick={onGenerate} disabled={isLoading}>Re-run C Simulation</button>
        <button className="btn" onClick={onPrevRound} disabled={isLoading || currentFrame <= 0}>Back</button>
        <button className="btn" onClick={onNextRound} disabled={isLoading || currentFrame >= lastFrame}>Step</button>
        <button className={isPlaying ? 'btn danger' : 'btn success'} onClick={togglePlay} disabled={isLoading || totalFrames <= 1}>
          {isPlaying ? 'Pause' : 'Play'}
        </button>
      </div>

      <div className="timeline-box">
        <div className="timeline-meta">
          <span>Playback</span>
          <strong>{currentFrame + 1}/{Math.max(totalFrames, 1)}</strong>
        </div>
        <input
          type="range"
          min="0"
          max={lastFrame}
          value={currentFrame}
          onChange={(e) => onJump(e.target.value)}
          disabled={totalFrames <= 1}
        />
        <select value={playSpeed} onChange={(e) => setPlaySpeed(Number(e.target.value))}>
          <option value={1500}>Slow</option>
          <option value={900}>Normal</option>
          <option value={350}>Fast</option>
        </select>
      </div>
    </div>
  );
}
