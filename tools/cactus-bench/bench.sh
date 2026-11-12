#!/system/bin/sh
# bench.sh <depth> <threads>
cd /data/local/tmp || exit 1
export NEEDLE_TELEMETRY=0 DO_NOT_TRACK=1 CACTUS_TELEMETRY=0
DEPTH=${1:-20}
THREADS=${2:-4}
SYS="date: 2026-11-12; device: RG405V handheld; current profile: stock; fan: auto; battery: 78%"
while IFS= read -r p; do
  [ -z "$p" ] && continue
  t0=$(awk '{print $1}' /proc/uptime)
  out=$(./cactus --model needle3.cact --tools tools.json --system "$SYS" \
        --prompt "$p" --max 256 --depth "$DEPTH" --threads "$THREADS" 2>&1)
  t1=$(awk '{print $1}' /proc/uptime)
  ms=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%d", (b-a)*1000}')
  printf '%s\t%s\t%s\n' "$ms" "$p" "$out"
done < prompts.txt
