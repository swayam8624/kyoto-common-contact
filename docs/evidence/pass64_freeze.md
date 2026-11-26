# Pass 64 — final CPU research freeze

Use the downloadable final evidence ZIP identified by pass64_final_audit.json. The ZIP contains 19 prior research archives, 5 original CI timing artifacts, fresh reproduction outputs, strict audit code, pinned dependencies, and reproduce.sh. Run bash reproduce.sh after installing requirements.txt in a fresh extracted directory. Verify SHA256SUMS before execution. Strict byte/pixel parity is scoped to the tested Linux environment; a different machine may fail those checks without indicating a physical error.

The preferred CPU candidate backend is the original Fused-axis SAP for tested workloads; the independent physical reference is Pass56 duration-parametric FEM with the exact whole-mesh gate. They are separate validated code paths. Keep Pass57/58 alternatives as experimental negative results. Do not merge into main automatically. No claim of universal speed, full IPC solver superiority, population inference, or independent novelty.

Actual Metal device execution and native macOS confirmation are still open. Run the mandatory hardware workflow .github/workflows/pass60-metal.yml on a physical Apple machine with the archived Pass47 inputs. A skipped device or scalar fallback cannot close the gate.
