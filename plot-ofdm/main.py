import argparse
import os
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def load_complex_csv(input_dir: Path, name: str) -> np.ndarray:
    data = np.loadtxt(input_dir / name, delimiter=",", skiprows=1)
    return data[:, 1] + 1j * data[:, 2]


def plot_complex(ax, arr: np.ndarray, title: str, connect: bool = True) -> None:
    idx = np.arange(arr.shape[0])
    linestyle = "-" if connect else "None"
    ax.plot(
        idx,
        arr.real,
        marker="o",
        markersize=2.5,
        linewidth=1.0,
        linestyle=linestyle,
        label="Real",
    )
    ax.plot(
        idx,
        arr.imag,
        marker="x",
        markersize=2.5,
        linewidth=1.0,
        linestyle=linestyle,
        label="Imag",
    )
    ax.set_title(title, fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right", fontsize=8)


def plot_constellation(ax, arr: np.ndarray, title: str) -> None:
    ax.scatter(arr.real, arr.imag, s=20, alpha=0.8, label="Symbols")
    ref = np.array([1 + 1j, 1 - 1j, -1 + 1j, -1 - 1j]) / np.sqrt(2.0)
    ax.scatter(ref.real, ref.imag, marker="x", s=70, linewidths=2, label="Ideal QPSK")
    ax.axhline(0.0, color="gray", linewidth=0.8)
    ax.axvline(0.0, color="gray", linewidth=0.8)
    ax.set_aspect("equal", adjustable="box")
    ax.set_title(title, fontsize=10)
    ax.set_xlabel("In-phase")
    ax.set_ylabel("Quadrature")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right", fontsize=8)


def plot_stem_complex(ax, arr: np.ndarray, title: str) -> None:
    idx = np.arange(arr.shape[0])
    m1, s1, _ = ax.stem(idx, arr.real, linefmt="C0-", markerfmt="C0o", basefmt=" ")
    m2, s2, _ = ax.stem(idx, arr.imag, linefmt="C1-", markerfmt="C1x", basefmt=" ")
    plt.setp(s1, linewidth=1.0)
    plt.setp(s2, linewidth=1.0)
    plt.setp(m1, markersize=4)
    plt.setp(m2, markersize=4)
    ax.set_title(title, fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.legend([m1, m2], ["Real", "Imag"], fontsize=8, loc="upper right")

def default_input_dir() -> Path:
    script_dir = Path(__file__).resolve().parent
    candidates = [
        script_dir / "output",
        script_dir.parent / "output",
        script_dir.parent / "simulate-ofdm" / "output",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return script_dir.parent / "output"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Plot OFDM block-by-block signal evolution from CSV outputs."
    )
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=default_input_dir(),
        help="Directory containing OFDM CSV files (default: auto-detected output dir).",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output PNG path (default: <input-dir>/ofdm_blocks.png).",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Directory for the combined plot and individual plot images (default: <input-dir>).",
    )
    parser.add_argument(
        "--dpi",
        type=int,
        default=150,
        help="Output image DPI (default: 150).",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Show figure window after saving.",
    )
    return parser


def main() -> None:
    args = build_parser().parse_args()
    input_dir = args.input_dir
    if not input_dir.exists():
        raise FileNotFoundError(f"Input directory does not exist: {input_dir}")

    output_dir = args.output_dir if args.output_dir is not None else input_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    tx_freq = load_complex_csv(input_dir, "tx_freq_symbols.csv")
    tx_time = load_complex_csv(input_dir, "tx_time_ifft.csv")
    tx_cp = load_complex_csv(input_dir, "tx_with_cp.csv")
    h = load_complex_csv(input_dir, "channel_impulse.csv")
    rx_ch = load_complex_csv(input_dir, "rx_after_channel.csv")
    rx_noisy = load_complex_csv(input_dir, "rx_after_noise.csv")
    rx_no_cp = load_complex_csv(input_dir, "rx_no_cp.csv")
    rx_freq = load_complex_csv(input_dir, "rx_freq_fft.csv")
    h_freq = load_complex_csv(input_dir, "channel_freq.csv")
    eq_freq = load_complex_csv(input_dir, "equalized_freq.csv")
    ml = load_complex_csv(input_dir, "ml_decision.csv")

    fig, axes = plt.subplots(4, 3, figsize=(17, 16), constrained_layout=True)
    axes = axes.ravel()

    plot_complex(axes[0], tx_freq, "1) QPSK symbols D[k]", connect=False)
    plot_complex(axes[1], tx_time, "2) IFFT d[n] (time domain)")
    plot_complex(axes[2], tx_cp, "3) Add cyclic prefix")
    axes[2].axvspan(0, 15, alpha=0.18, color="tab:orange", label="CP region")
    axes[2].legend(loc="upper right", fontsize=8)

    plot_stem_complex(axes[3], h, "4) Channel impulse h[n]")
    plot_complex(axes[4], rx_ch, "5) After channel (noiseless)")
    plot_complex(axes[5], rx_noisy, "6) After AWGN")
    plot_complex(axes[6], rx_no_cp, "7) Remove cyclic prefix")
    plot_complex(axes[7], rx_freq, "8) FFT R[k]")

    k = np.arange(h_freq.shape[0])
    axes[8].plot(k, np.abs(h_freq), marker="o", markersize=2.3, linewidth=1.0, label="|H[k]|")
    axes[8].plot(
        k,
        np.unwrap(np.angle(h_freq)),
        marker="x",
        markersize=2.3,
        linewidth=1.0,
        label="angle(H[k])",
    )
    axes[8].set_title("9) Channel response magnitude/phase", fontsize=10)
    axes[8].grid(True, alpha=0.3)
    axes[8].legend(loc="upper right", fontsize=8)
    axes[8].set_xlabel("Subcarrier index (k)")
    axes[8].set_ylabel("Magnitude / Phase (rad)")

    plot_constellation(axes[9], tx_freq, "10) Tx constellation D[k]")
    plot_constellation(axes[10], eq_freq, "11) Equalized constellation")
    plot_constellation(axes[11], ml, "12) ML decision constellation")

    for i in [0, 1, 4, 5, 6, 7]:
        axes[i].legend(loc="upper right", fontsize=8)

    out_png = args.output if args.output is not None else input_dir / "ofdm_blocks.png"
    out_png.parent.mkdir(parents=True, exist_ok=True)
    fig.suptitle("OFDM Block-by-Block Signal Evolution (QPSK)", fontsize=14)
    fig.savefig(out_png, dpi=args.dpi)
    plt.close(fig)

    individual_plots = [
        ("01_tx_freq_symbols.png", lambda ax: plot_complex(ax, tx_freq, "1) QPSK symbols D[k]", connect=False)),
        ("02_tx_time_ifft.png", lambda ax: plot_complex(ax, tx_time, "2) IFFT d[n] (time domain)")),
        (
            "03_tx_with_cp.png",
            lambda ax: (
                plot_complex(ax, tx_cp, "3) Add cyclic prefix"),
                ax.axvspan(0, 15, alpha=0.18, color="tab:orange", label="CP region"),
                ax.legend(loc="upper right", fontsize=8),
            ),
        ),
        ("04_channel_impulse.png", lambda ax: plot_stem_complex(ax, h, "4) Channel impulse h[n]")),
        ("05_rx_after_channel.png", lambda ax: plot_complex(ax, rx_ch, "5) After channel (noiseless)")),
        ("06_rx_after_noise.png", lambda ax: plot_complex(ax, rx_noisy, "6) After AWGN")),
        ("07_rx_no_cp.png", lambda ax: plot_complex(ax, rx_no_cp, "7) Remove cyclic prefix")),
        ("08_rx_freq_fft.png", lambda ax: plot_complex(ax, rx_freq, "8) FFT R[k]", connect=False)),
        (
            "09_channel_response.png",
            lambda ax: (
                ax.plot(k, np.abs(h_freq), marker="o", markersize=2.3, linewidth=1.0, label="|H[k]|") ,
                ax.plot(
                    k,
                    np.unwrap(np.angle(h_freq)),
                    marker="x",
                    markersize=2.3,
                    linewidth=1.0,
                    label="angle(H[k])",
                ),
                ax.set_title("9) Channel response magnitude/phase", fontsize=10),
                ax.grid(True, alpha=0.3),
                ax.legend(loc="upper right", fontsize=8),
            ),
        ),
        ("10_tx_constellation.png", lambda ax: plot_constellation(ax, tx_freq, "10) Tx constellation D[k]")),
        ("11_equalized_constellation.png", lambda ax: plot_constellation(ax, eq_freq, "11) Equalized constellation")),
        ("12_ml_decision_constellation.png", lambda ax: plot_constellation(ax, ml, "12) ML decision constellation")),
    ]

    for filename, plotter in individual_plots:
        individual_fig, individual_ax = plt.subplots(figsize=(6.5, 4.8), constrained_layout=True)
        plotter(individual_ax)
        individual_fig.savefig(output_dir / filename, dpi=args.dpi)
        plt.close(individual_fig)

    if args.show and os.environ.get("MPLBACKEND", "").lower() != "agg":
        plt.show()
    print(f"Saved plot: {out_png}")


if __name__ == "__main__":
    main()
