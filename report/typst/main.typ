#let sans = ("Noto Sans KR", "Noto Sans CJK KR", "Noto Sans JP", "Noto Sans CJK JP")
#let serif = ("Noto Serif KR", "Noto Serif CJK KR", "Noto Serif JP", "Noto Serif CJK JP")
#let mono = ("Noto Sans Mono", "D2Coding", "MonoplexKR")

#set text(font: serif, size: 10pt)

#counter(page).update(1)

// Required End

#set page(margin: (
  top: 25mm,
  bottom: 25mm,
  left: 25mm,
  right: 25mm,
),
  numbering: "1"
)

// #show raw: it => text(font: mono)[#it]

#set text(font: serif, size: 10pt)
#show heading.where(
  level: 1
): it => block(width: 100%)[
  #set align(center)
  #text(weight: "regular", size: 1.3em)[
    #it.body
  ]
]

#set heading(numbering: "1.")

#show heading: set block(spacing: 1.2em)
#show heading.where(level: 1): set align(center)
#show heading.where(level: 1): set text(font: sans, size: 1.3em, weight: "medium")
#show heading.where(level: 2): set text(font: sans, size: 1.3em, weight: "regular")

#show quote.where(block: true): set align(center)

#let img(path, size: 100%) = {
  align(center)[
    #image(path, width: size)
  ]
}

#let hero(
  title,
  subtitle,
) = [
  #place(
    top + center,
    float: true,
    scope: "parent",
    clearance: 30pt,
  )[
    #text(size: 2.4em)[#par(leading: .7em, spacing: 0em)[#title]]
    #text(size: 1.5em)[#par(spacing: 1em)[#subtitle]]
    
    #align(center)[#text(12pt)[#par(leading: .7em)[
      박종현, 
      전남대학교 공과대학 컴퓨터정보통신공학과\
      jonghyeon\@jnu.ac.kr
    ]]]
  ]
]

#show raw.where(block: false): it => box(fill: rgb("f5f5f5"), outset: (y: 3pt), inset: (x: 2pt), text(fill: red, it))
#show raw.where(block: true): it => block(fill: rgb("f5f5f5"), inset: (x: 10pt, y: 10pt), width: 100%, it)

#set par(justify: true, leading: 1.1em, spacing: 2em)

#set footnote.entry(gap: 1em)
#show footnote.entry: set par(leading: 1em, spacing: 1em)

#set table(stroke: 0.5pt + luma(25%))

/**
 * Content Start
 */

#hero(
  [
    CP-OFDM 송수신기의 시뮬레이션과\
    송수신 신호의 변화 추적
  ],
  [〈모바일통신시스템〉과제]
)

\

#align(center)[#box(width: 90%)[#text(font: sans)[
  이 프로젝트에서는 QPSK 변조를 사용하는 OFDM 송수신 체인을 구현하고, 신호를 처리하는 각 중간 단계에서 신호가 어떻게 변화하는지 구체적인 값을 확인할 수 있다. 송수신 처리와 중간 상황의 시뮬레이션 작업을 프로그램으로 구현하여, 개별 신호의 값을 획득, 중간과정을 포함하여 이들 값을 도표로 정리하였다. 시뮬레이션 시스템은 Subcarrier(부반송파), Cyclic Prefix, 다중경로 채널, AWGN 잡음 환경을 고려하여 시뮬레이션할 수 있다.

  \
  OFDM, QPSK, Cyclic Prefix, FFT, Channel Equalization, AWGN, SER, BER
]]]

\

= 1. 도입

OFDM(Orthogonal Frequency Division Multiplexing)은 디지털 통신 시스템에서 널리 사용되는 변조 및 다중화 방식이다. 전체 전송 대역을 다수의 subcarrier로 나누고, 각 subcarrier에 독립적인 심볼을 실어 전송한다. 이렇게 하여 frequency selective-fading 채널을 다수의 평탄 페이딩 부채널로 분해할 수 있게 하며, 수신단에서 단순한 1-tap 등화만으로 채널 왜곡을 보상할 수 있다.  

본 프로젝트는 OFDM 송수신 과정을 QPSK에 대해 적용하고, @system-overview 구성한 시스템의 각 블록에서 신호가 어떻게 변화하는지를 plot으로 확인하기 위해 작성하였다. QPSK를 사용하여 송신, 채널, 수신 각 블록의 신호 변화를 시각적으로 확인하기 위해 중간 과정의 구체적인 값을 획득할 수 있게 하였고, 이들 결과를 그래프로 시각화하여 이어지는 내용에 함께 첨부하였다.

#place(top + center, float: true)[#figure(image("images/system-overview.drawio.png", width: 50%), caption: [시뮬레이션 시스템의 처리 흐름])<system-overview>]

\

= 2. 시뮬레이션 구성

이 시뮬레이션에서는 @experiment-config 와 같이 구성하여 OFDM 환경을 시뮬레이션하였다. 다시 말해 지연 $0$, $2$, $4$ 위치에 각각 크기 $1$, $0.5$, $0.3$의 다중경로 성분이 존재하고, 채널은 최대 $L_h - 1 = 4$ 지연되어, 잡음이 없고 타이밍 동기가 정확할 때, 수신된 OFDM 심볼에서 CP를 제거하면 원형 컨볼루션 관계를 만족해, FFT 이후에 subcarrier별 단순 곱셈 모델로 표현할 수 있다.

#place(top + center, float: true)[#figure(table(columns: 2, align: (left, right),
  inset: .6em,
  align(center)[항목], align(center)[값],
  [subcarrier 수], [$N = 64$],
  [Cyclic Prefix 길이], [$"CP" = 16$],
  [채널 임펄스 응답 길이], [$L_h = 5$],
  [채널 임펄스 응답], [$h[n] = [1, 0, 0.5, 0, 0.3]$],
  [변조 방식], [QPSK],
  [QPSK 심볼], [$(plus.minus 1 plus.minus j) \/ sqrt(2)$],
  [AWGN], [$"SNR" = 10$ dB],
  [난수 시드], [`srand(42)`],
), caption: [시뮬레이션 시스템에서 사용한 파라미터])<experiment-config>]

== 송수신 신호 모델

주파수 영역 송신 심볼을 $D[k]$라고 할 때, IFFT를 통해 시간 영역 OFDM 심볼 $d[n]$을 생성할 수 있다.

$
d[n] = 1 / N sum_(k = 0)^(N - 1) D[k] e^(j 2 pi k n \/ N)
$

이 심볼 $d[n]$에 CP를 삽입한 송신 신호 $d_"CP" [n]$은 다음과 같다.  

$
d_"CP" [n] = [d[N - "CP"], dots, d[N - 1], d[0], dots, d[N - 1]]
$

채널 통과 후 수신 신호는 선형 컨볼루션 $r_"ch" [n]$ 으로, 혹은 AWGN을 통과한 경우 복소 가우시안 잡음 $w[n]$ 에 대해서 $r_"noisy" [n]$ 으로 표현된다.  

$
r_"ch" [n] = d_"CP" [n] convolve h[n]
$

$
r_"noisy" [n] = r_"ch" [n] + w[n]
$

CP 제거 후의 FFT 입력 신호는 $r[n]$ 과 같다.

$
r[n] = r_"noisy" [n + "CP"], quad n = 0, dots, N - 1
$

만약 $"CP" >= L_h - 1$이라면, 수신기에서 CP를 제거하고 FFT를 수행했을 때, 각 subcarrier $k$에 대해 이후 수신된 심볼 $R[k]$, 채널의 주파수 응답 $H[k]$, 전송된 심볼 $D[k]$ 사이에 아래와 같은 관계가 성립된다. $H[k]$ 는 채널 임펄스 응답을 $N$만큼 zero-padding 하여 FFT한 값이다.

$
H[k] = op("FFT"){h[n]}
$

$
R[k] = H[k] D[k]
$

이 때 채널 응답 $H[k]$가 알려져있으면 각 subcarrier마다 단 하나의 복소수 계수만을 사용해 신호를 복원할 수 있다.

$
hat(D)[k] = (R[k]) / (H[k])
$

채널 감쇠가 매우 작거나 0에 가까운 경우 $H[k]$로 나누면 수치적으로 불안정해질 수 있다. 때문에 $epsilon$을 추가하여 복소 켤레 $H^*[k]$, 전력 $|H[k]|^2$에 대해서 아래와 같이 변형해 사용한다.

$
hat(D)[k] = (R[k] H^*[k]) / (|H[k]|^2 + epsilon)
$

잡음이 없고, $epsilon$ 이 매우 작으며 $H[k]$ 가 $0$이 아니면 $hat(D)[k]$는 $D[k]$에 근사한다.

$
hat(D)[k] approx D[k]
$

QPSK Hard Decision은 실수부와 허수부의 부호를 이용해 판단하도록 정의한다. 

$
tilde(D)[k] = (op("sgn")(Re(hat(D)[k])) + j op("sgn")(Im(hat(D)[k]))) / sqrt(2)
$

= 3. 구현

== FFT/IFFT의 구현

FFT/IFFT 블록의 입출력 관계는 DFT/IDFT로 구현하였다.

```c
void dft(const double complex *in, double complex *out, int n, int inverse) {
  for (int k = 0; k < n; ++k) {
    double complex acc = 0.0 + 0.0 * I;
    for (int t = 0; t < n; ++t) {
      double angle = 2.0 * M_PI * (double)k * (double)t / (double)n;
      double complex w = cos(angle) + (inverse ? I : -I) * sin(angle);
      acc += in[t] * w;
    }
    out[k] = inverse ? (acc / (double)n) : acc;
  }
}
```

`inverse = 0`인 경우, 음의 지수를 사용하고 별도의 scaling을 적용하지 않아 FFT를 수행한다.

$
X[k] = sum_(n = 0)^(N - 1) x[n]e^(-j 2 pi k n \/N)
$

`inverse = 1`인 경우, 양의 지수를 사용하고 $1/N$ scaling을 적용하여 IFFT가 수행된다.

$
x[n] = 1/N sum_(k = 0)^(N - 1) X[k] e^(j 2 pi k n \/N)
$

== QPSK 심볼 생성

QPSK 심볼은 $I$축과 $Q$축의 부호를 각각 독립적으로 선택하여 생성한다.

```c
for (int k = 0; k < N; ++k) {
  int b_i = (rand() % 2) ? 1 : -1;
  int b_q = (rand() % 2) ? 1 : -1;
  tx_freq[k] = (b_i + I * b_q) / sqrt(2.0);
}
```

이 때 가능한 심볼은 다음 네 점이다: $(1 + j)/sqrt(2)$, $(1 - j)/sqrt(2)$, $(-1 + j)/sqrt(2)$, $(-1 - j)/sqrt(2)$ 이 심볼 각각의 에너지는 아래와 같이 계산되어 평균 심볼 에너지 $1$로 정규화됨을 알 수 있다.  

$
| (plus.minus 1 plus.minus j) / sqrt(2) |^2 = (1^2 + 1^2) / 2 = 1
$

== Cyclic Prefix <heading-cyclic-prefix>

OFDM 시간 영역 심볼 $d[n]$의 마지막 16개 샘플을 앞에 복사하여 CP로서 삽입한다. CP 삽입 후 신호의 길이는 $N + "CP" = 64 + 16 = 80$ 이다.

```c
for (int i = 0; i < CP; i++) { tx_cp[i] = tx_time[N - CP + i]; }
for (int i = 0; i < N; i++) { tx_cp[CP + i] = tx_time[i]; }
```

CP는 다중경로 채널에서 발생하는 심볼 간 간섭을 방지하고, 수신 FFT 구간 내에서 선형 컨볼루션을 원형 컨볼루션처럼 보이게 하기 위해 사용한다. 본 시스템에서는 채널 최대 지연이 4샘플이고 CP 길이가 16샘플이므로 목적에 맞게 심볼을 보호할 수 있을 것으로 판단한다.

== 채널 컨볼루션

채널 통과는 선형 컨볼루션으로 구현한다.

```c
void convolve(
  const double complex *x, int nx, const double complex *h, int nh, double complex *y
) {
  int ny = nx + nh - 1;
  for (int n = 0; n < ny; n++) {
    double complex acc = 0.0 + 0.0 * I;
    for (int k = 0; k < nh; k++) {
      int xi = n - k;
      if (xi >= 0 && xi < nx) {
        acc += x[xi] * h[k];
      }
    }
    y[n] = acc;
  }
}
```

입력 신호는 @heading-cyclic-prefix 에서 확인한 것과 같이 길이 $80$이고, 채널 길이는 $5$이므로, 컨볼루션의 출력 길이는 $80 + 5 - 1 = 84$ 이다.  

```c
const int rx_len = (N + CP) + CH_LEN - 1;
```

채널 임펄스 응답은 다음과 같이 정의했는데, 1개의 직접 경로(인덱스 0)와 2개의 지연 반사 경로(인덱스 2, 인덱스 4)를 갖는 frequency-selective fading으로 이해할 수 있다.

```c
double complex h[CH_LEN] = {
  1.0 + 0.0 * I,
  0.0 + 0.0 * I,
  0.5 + 0.0 * I,
  0.0 + 0.0 * I,
  0.3 + 0.0 * I,
};
```

== AWGN 통과

이 구현에서는 AWGN을 구성 설정할 경우, 잡음 변수 $sigma_w^2$는 다음과 같다.

$
sigma_w^2 = P_"signal"/(10^("SNR"_"dB"\/10))
$

```c
double noise_var = 0.0;
if (snr_db >= 0.0) {
    double snr_linear = pow(10.0, snr_db / 10.0);
    noise_var = signal_power / snr_linear;
}
```

AWGN은 복소공간에 적용하게 되는데, 실수부와 허수부에 각각 독립적으로 노이즈를 더함으로써 적용했다. 

```c
double sigma = sqrt(noise_var / 2.0);
double n_re = sigma * gaussian_rand();
double n_im = sigma * gaussian_rand();
rx_noisy[i] = rx_ch[i] + (n_re + I * n_im);
```

노이즈의 생성은 Box-Muller 변환을 사용하였다.

$
X = sqrt(-2 ln U_1) cos (2 pi U_2)
$

```c
double gaussian_rand(void) {
  double u1 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
  double u2 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
  return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}
```

== 수신 측에서 CP 제거 및 FFT

수신부에서 CP를 제거하고 $N = 64$의 샘플을 FFT에 입력한다. 이 구현에서는 직접 경로의 도착 시점을 기준으로 수신 타이밍이 정확히 동기화되어 있다고 가정하므로, 컨볼루션 출력에서 인덱스 $"CP"$부터 $"CP"+N-1$까지의 $N$개 샘플을 FFT 입력 구간으로 사용한다.

```c
for (int i = 0; i < N; i++) { rx_no_cp[i] = rx_noisy[CP + i]; }
/* ... */
dft(rx_no_cp, rx_freq, N, 0);
```

CP의 길이가 채널의 최대 지연 이상이기 때문에 잡음이 없는 경우 FFT 결과는 다음의 관계를 만족한다.

$
R[k] = H[k]D[k]
$

== 채널 주파수 응답 계산

길이 $L_h$ 5의 채널 임펄스 응답 $h[n]$에 대해 FFT를 수행해 $H[k]$를 획득한다.

```c
double complex h_pad[N];
for (int i = 0; i < N; ++i) {
  h_pad[i] = (i < CH_LEN) ? h[i] : 0.0 + 0.0 * I;
}

/* ... */

double complex h_freq[N];
dft(h_pad, h_freq, N, 0);
```

채널 주파수 응답은 각 subcarrier에서의 채널 이득을 나타낸다. $|H[k]|$는 개별 subcarrier $H[k]$의 진폭 변화, $angle H[k]$는 위상 회전이다.  

$
H[k] = |H[k]| e^(j angle H[k])
$

== 수신부에서의 1-tap 등화

수신부에서의 등화는 다음과 같이 이루어진다.

$
hat(D)[k] = (R[k]H^*[k]) / (|H[k]|^2 + epsilon)
$

```c
double h2 = pow(cabs(h_freq[k]), 2.0);
eq_freq[k] = (rx_freq[k] * conj(h_freq[k])) / (h2 + EQ_EPS);
```

위 정의는 zero-forcing equalizer를 변형한 것이다.

$
hat(D)[k]
= R[k] / H[k]
= (R[k] H^*[k]) / (H[k] H^*[k])
= (R[k] H^*[k]) / (|H[k]|^2)
$


== Hard Decision

등화된 심볼 $hat(D)[k]$ 는 네 개의 이상적인 QPSK 점 중 하나로 판정한다.

```c
double d_re = (creal(eq_freq[k]) >= 0.0) ? (1.0 / sqrt(2.0)) : (-1.0 / sqrt(2.0));
double d_im = (cimag(eq_freq[k]) >= 0.0) ? (1.0 / sqrt(2.0)) : (-1.0 / sqrt(2.0));
decided[k] = d_re + I * d_im;
```

이렇게 판정된 점이 송신 시의 QPSK 점과 상이한 것인지 검사하는데, 심볼 오류는 송신 심볼과 판정 결과가 다를 때, 비트 오류는 QPSK의 $I$축과 $Q$축의 부호를 각각 1비트로 보고 계산한다.

```c
if (cabs(decided[k] - tx_freq[k]) > 1e-9) { symbol_errors++; }

if ((creal(decided[k]) >= 0.0) != (creal(tx_freq[k]) >= 0.0)) { bit_errors++; }
if ((cimag(decided[k]) >= 0.0) != (cimag(tx_freq[k]) >= 0.0)) { bit_errors++; }
```

이와 같이 수집한 오류 사례들에 대해서, SER과 BER을 계산할 수 있다. QPSK는 한 심볼에 2비트가 대응되므로, BER 계산 시의 전체 비트 수는 $2N$이다.

$
"SER" = N_"sym,err" / N, quad "BER" = N_"bit,err"/(2N)
$

= 4. 실험 및 결과

#let plot-single-width =  60%

앞서 제시한 @experiment-config 와 같이 시뮬레이션을 구성, 수행하여 시뮬레이션 중간 과정 중에 다루어지는 값을 획득하였다. 이들 값은 아래와 같았다.

== QPSK 심볼 $D[k]$과 $D[k]$를 IFFT한 $d[n]$

#place(top + center, float: true)[#grid(
  columns: (1fr, 1fr),
  inset: .5em,
  [#figure(image("images/plots/01_tx_freq_symbols.png"), caption: [임의 심볼을 생성하여 각 subcarrier에 할당한 결과 QPSK 심볼 $D[k]$])<fig-plot-tx-gen>],
  [#figure(image("images/plots/02_tx_time_ifft.png"), caption: [$D[k]$를 IFFT 한 결과 $d[n]$])<fig-plot-tx-ifft>]
)]

주파수 영역 송신 QPSK 심볼의 실수부, 허수부 값은 각 subcarrier $k$에 대해 각각 $plus.minus 1/sqrt(2)$이다.(@fig-plot-tx-gen 참고)

IFFT를 거친 $D[k]$인 $d[n]$은 모든 subcarrier가 합성된 복소 시간 영역 파형이다. (@fig-plot-tx-ifft 참고)  

$
d[n] = 1/N sum_(k=0)^(N - 1) D[k]e^(j 2 pi k n\/N)
$

== Cyclic Prefix

Cyclic Prefix를 추가하여 @fig-plot-tx-ifft 의 뒷 16개 부분 샘플이 그래프 앞부분에 반복되는 @fig-plot-tx-cp 가 된 것을 확인할 수 있다. CP는 수신단에서 제거되나, 채널 지연에서 유발되는 간섭을 흡수하고, FFT 구간이 원형 컨볼루션 관계를 만족하도록 하는 역할을 한다.

== Channel Impulse $h[n]$

$
h[n] = [1, 0, 0.5, 0, 0.3]
$

채널 임펄스 응답은 @fig-chan-impulse-resp 와 같이 구성하였다. $n=0$에 직접 경로, $n = 2$, $n = 4$에 반사 경로가 존재하는 multipath 채널이다. 시간 영역에서는 지연된 신호들이 합쳐지는 효과, 주파수 영역에서는 각 subcarrier마다 서로 다른 진폭과 위상 변화를 발생시킨다.  

#place(top + center, float: true)[#grid(
  columns: (1fr, 1fr),
  inset: .5em,
  [#figure(image("images/plots/03_tx_with_cp.png"), caption: [@fig-plot-tx-ifft 에 Cyclic Prefix를 추가한 결과])<fig-plot-tx-cp>],
  [#figure(image("images/plots/04_channel_impulse.png"), caption: [채널 임펄스 응답의 구성])<fig-chan-impulse-resp>]
)]

== 채널과 AWGN의 통과

#place(top + center, float: true)[#grid(
  columns: (1fr, 1fr),
  inset: .5em,
  [#figure(image("images/plots/05_rx_after_channel.png"), caption: [채널을 통과하여 수신 측에서 확인 가능할 신호 (AWGN 없음)])<fig-plot-rx-after-chan>],
  [#figure(image("images/plots/06_rx_after_noise.png"), caption: [채널을 통과하여 수신 측에서 확인 가능할 신호 (AWGN 추가)])<fig-plot-rx-awgn>],
  grid.cell(colspan: 2)[#figure(image("images/plots/07_rx_no_cp.png", width: 50%), caption: [CP를 제거한 노이즈 포함된 수신 신호])<fig-plot-rx-remove-cp>]
)]

CP가 포함된 송신 신호가 채널과 AWGN을 통과한 후의 신호는 각각 @fig-plot-rx-after-chan, @fig-plot-rx-awgn 과 같다. 이 신호는 선형 컨볼루션 결과이므로 길이가 84로 증가한다.

채널의 다중경로 성분으로 인해 원래 신호의 지연 복사본들이 더해진 형태가 된다. 시간 영역 파형은 송신 CP 포함 신호와 다르게 왜곡된다. AWGN 채널을 통과한 후에는 복소 Gaussian noise가 더해져 real/imag 파형에 불규칙한 변동이 추가된다. SNR이 낮을수록 이 변동은 커진다.

== Cyclic Prefix 제거

수신한 신호에 대해서, CP를 제거하고 $N=64$개의 신호 샘플을 추출한 결과 @fig-plot-rx-remove-cp 와 같은 신호를 획득할 수 있다.

$
r[n] = r_("ch"\/"noisy")[n+"CP"], quad n = 0, dots, N - 1
$

CP가 충분히 길어서, 이 구간은 채널과의 원형 컨볼루션 관계를 만족한다. FFT를 수행하면 각 subcarrier에서 단순 곱셈 모델을 획득할 수 있다.

== FFT $R[k]$

CP를 제거한 신호를 FFT했을 때, 잡음이 없는 경우 아래의 관계를 만족하므로, 송신 QPSK 심볼 $D[k]$에 채널 주파수 응답 $H[k]$가 곱해진 형태로 이해할 수 있다. $R[k]$는 원래의 QPSK의 네 점에 위치하지 않고 subcarrier별로 크기와 위상이 달라진 복소 값으로 나타난다.

$
R[k] = H[k]D[k]
$

#place(top + center, float: true)[#grid(
  columns: (1fr, 1fr),
  inset: .5em,
  [#figure(image("images/plots/01_tx_freq_symbols.png"), caption: [Tx에서 생성한 임의 심볼 $D[k]$ (@fig-plot-tx-gen)])<fig-plot-tx-gen-repeat>],[#figure(image("images/plots/08_rx_freq_fft.png"), caption: [Rx에서 수신한 심볼])<fig-plot-freq-fft>]
)]

== Channel response magnitude와 phase

#place(top + center, float: true)[#figure(image("images/plots/09_channel_response.png", width: plot-single-width), caption: [$H(k)$의 magnitude와 phase])<fig-plot-magnitude-and-phase>]

$H[k]$의 magnitude와 phase는 @fig-plot-magnitude-and-phase 와 같다. $|H[k]|$가 큰 subcarrier에서는 신호가 상대적으로 증폭되고, 작은 subcarrier에서는 감쇠된다. $angle H[k]$는 각 subcarrier에서 발생하는 위상 회전이다. 등화기는 각 subcarrier에서 이 크기 변화와 위상 회전을 반대로 보상하여 송신 심볼을 복원한다.

$
H[k] = |H[k]| e^(j angle H[k])
$

$H[k]$를 이용하여 각 subcarrier마다의 왜곡을 보상하도록 하여 보정한다.

== Tx constellation $D[k]$와 Equalized constellation $hat(D)[k]$

이상적인 QPSK 기준점은 아래의 네 점이다. 송신 심볼이 이들 네 점 중 하나의 점에 위치, QPSK로 매핑되었다. (@fig-plot-tx-constellation)

$
(1 + j)/sqrt(2), quad (1 - j)/sqrt(2), quad (-1 + j)/sqrt(2), quad (-1 - j)/sqrt(2)
$

잡음이 없는 경우, 수신 측은 송신 측의 QPSK 점과 거의 일치한다. 하지만 잡음이 있는 경우, 다음과 같이 잡음 성분이 포함된다. 따라서 각 심볼은 이상적인 QPSK 점을 중심으로 주변에 퍼진 형태로 나타난다. SNR이 낮을수록 더 넓게 분포되고, $|H[k]|$가 작은 subcarrier에서는 잡음의 증폭 효과가 더 크게 나타날 수 있다. (@fig-plot-rx-constellation)

$
hat(D)[k] = D[k] + (W[k]H^*[k])/(|H[k]|^2+epsilon)
$

#place(top + center, float: true)[#grid(
  columns: (1fr, 1fr),
  inset: .5em,
  [#figure(image("images/plots/10_tx_constellation.png"), caption: [송신 측 심볼의 QPSK constellation])<fig-plot-tx-constellation>],
  [#figure(image("images/plots/11_equalized_constellation.png"), caption: [수신 측 심볼의 Equalized constellation])<fig-plot-rx-constellation>],
)]

잡음으로 수신 측 점이 QPSK 점과 차이가 난다고 하더라도, 가장 가까운 QPSK 기준점으로 매핑하여 심볼을 복원할 수 있다. 다만 잡음이 있는 경우에도 decision boundary를 넘지 않은 심볼은 올바르게 판정되지만, 경계를 넘은 심볼은 다른 QPSK 점으로 잘못 판정되어 SER/BER가 증가한다. @experiment-config 과 같이 실험하고, SNR을 증가/감소시켜 변형 실험한 결과, 아래와 같은 결과를 획득할 수 있었다. @result-snr-5, @result-snr-10, @result-snr-20 을 비교하면 SNR이 증가할수록 SER과 BER이 감소하는 경향을 확인할 수 있다. AWGN 전력이 감소함에 따라 등화 후 constellation 점들이 이상적인 QPSK 기준점에 더 가깝게 모이기 때문이다. 특히 높은 SNR에서는 decision boundary를 넘는 심볼이 거의 발생하지 않아 오류율이 0에 가까워진다.


#align(center)[#grid(
  columns: (1fr, 1fr, 1fr),
  [#figure(
    table(
      columns: 2,
      align: (left, right),
      inset: .6em,
      [항목], [값],
      [SNR], [$5$ dB],
      [총 심볼 수], [$64$],
      [심볼 오류 수], [$11_"sym,err"$],
      [비트 오류 수], [$11_"bit,err"$],
      [SER], [$11_"sym,err" \/ 64$],
      [BER], [$11_"bit,err" \/ 128$],
    ),
    caption: [
      $"SNR"=5.0$dB로 구성한\
      실험 결과 표
    ]
  )<result-snr-5>],
  [#figure(
    table(
      columns: 2,
      align: (left, right),
      inset: .6em,
      [항목], [값],
      [SNR], [$10$ dB],
      [총 심볼 수], [$64$],
      [심볼 오류 수], [$2_"sym,err"$],
      [비트 오류 수], [$2_"bit,err"$],
      [SER], [$2_"sym,err" \/ 64$],
      [BER], [$2_"bit,err" \/ 128$],
    ),
    caption: [
      @experiment-config 으로 구성한\
      실험 결과 표
    ]
  )<result-snr-10>],
  [#figure(
    table(
      columns: 2,
      align: (left, right),
      inset: .6em,
      [항목], [값],
      [SNR], [$20$ dB],
      [총 심볼 수], [$64$],
      [심볼 오류 수], [$0_"sym,err"$],
      [비트 오류 수], [$0_"bit,err"$],
      [SER], [$0_"sym,err" \/ 64$],
      [BER], [$0_"bit,err" \/ 128$],
    ),
    caption: [
      $"SNR"=20.0$dB로 구성한\
      실험 결과 표
    ]
  )<result-snr-20>],
)]
