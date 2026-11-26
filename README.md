# SoulChord

Plugin VST3 tạo vòng hòa âm cho **Pop, R&B và Hip hop**, dùng được trong FL Studio (Windows). Chọn giọng và vòng hợp âm, đảo voicing, đảo bass, chèn passing chord, rồi nghe ngay bằng tiếng có sẵn hoặc kéo thả MIDI vào Piano roll.

## Cài đặt vào FL Studio

1. Vào tab **Actions** của repo, mở lần build mới nhất có dấu ✓ xanh, kéo xuống mục **Artifacts** và tải **SoulChord-Windows** (hoặc tải ở mục **Releases** nếu đã có bản phát hành).
2. Giải nén. Bên trong có thư mục `SoulChord.vst3` (plugin) và `SoulChord.exe` (bản chạy độc lập để test nhanh).
3. Copy **cả thư mục** `SoulChord.vst3` vào `C:\Program Files\Common Files\VST3\`.
4. Mở FL Studio, vào **Options → Manage plugins**, bấm **Find installed plugins** (hoặc *Start scan*), tìm `SoulChord` rồi tick ngôi sao để thêm vào danh sách.
5. Thêm vào Channel Rack: bấm **+**, chọn **SoulChord**.

> Windows có thể cảnh báo vì plugin chưa được ký số (code signing). Plugin tự build từ code của bạn nên an toàn; chọn *More info → Run anyway* nếu được hỏi khi chạy `SoulChord.exe`.

## Hai chế độ chơi

**Tự chạy theo tempo**: bấm Play trong FL, plugin tự đánh cả vòng đúng tempo của bài và lặp lại. Nút **Nghe thử** chạy vòng ngay cả khi FL đang dừng.
*Lưu ý: ở chế độ này nếu dùng Pattern, hãy để một nốt dài (hoặc một pattern trống đủ dài) cho channel SoulChord để FL giữ plugin chạy trong Song mode.*

**Mỗi phím 1 hợp âm**: mỗi phím trên bàn phím MIDI / phím máy tính / nốt trong Piano roll là một hợp âm hoàn chỉnh của giọng đang chọn:
- Phím trắng (theo giọng) → hợp âm diatonic: ở giọng C là C, Dm, Em, F, G, Am, Bdim
- Phím đen (ngoài giọng) → **dominant phụ** dẫn vào bậc phía trên (C# = A7 → Dm, F# = D7 → G, G# = E7 → Am) hoặc **hợp âm mượn** (A# = Bb)

## Các phần điều khiển

| Phần | Ý nghĩa |
|---|---|
| **Thể loại** | Pop / R&B / Hip hop. Đổi thể loại sẽ tự set màu hòa âm, voicing, nhịp, tiếng phù hợp |
| **Giọng / Thang âm** | 12 giọng × Trưởng, Thứ, Thứ hòa âm, Dorian, Mixolydian, Phrygian |
| **Vòng hòa âm** | Thư viện vòng theo thể loại, hoặc *Tự nhập / sửa vòng* |
| **Màu hòa âm** | Triad → add9 → 7th → 9th → 11th/13th. Cùng một vòng, vặn phải là nghe chất R&B / neo-soul |
| **Kiểu voicing** | Close, Open, Drop 2, Spread, Rootless (bỏ nốt gốc, kiểu jazz/R&B) |
| **Thế đảo** | Thế gốc, đảo 1, 2, 3 |
| **Đi giọng mượt** | Tự chọn thế đảo để các nốt di chuyển ít nhất (voice leading) |
| **Bass** | Nốt gốc, theo vòng (slash chord), nốt 3, nốt 5, pedal (giữ nốt chủ), đi mượt (walking) |
| **Passing chord** | Dominant phụ (V7/x), giảm chuyển tiếp (dim7), ii–V, tritone sub, hoặc tự động |
| **Nhịp điệu** | Ngân dài, móc đơn (Pop), lệch nhịp (R&B), stab (Hip hop) + rải hợp âm + swing |
| **Tiếng** | Piano, Rhodes, Pad, Bell (bass sub tự động đi kèm) |

## Ký hiệu tự nhập vòng

Gõ các bậc cách nhau bằng dấu cách hoặc gạch ngang, ví dụ `1 5 6 4` hay `1-5/1-6-4`.

| Ký hiệu | Ý nghĩa | Ví dụ ở giọng C |
|---|---|---|
| `1`…`7` | bậc của thang âm | `6` = Am |
| `b` / `#` trước số | giáng / thăng | `b7` = Bb, `b6` = Ab |
| `m` `M` `7` `o` `s2` `s4` sau số | thứ, trưởng, dominant 7, giảm 7, sus2, sus4 | `37` = E7, `4m` = Fm, `5s4` = Gsus4 |
| `/1` `/2` `/3` | bass là nốt 3 / 5 / 7 của hợp âm | `5/1` = G/B |
| `*2` … `*8` | độ dài gấp 2…8 lần | `1*2` |

Ví dụ: Canon bass đi xuống `1 5/1 6 3/1 4 1/1 2 5` → C – G/B – Am – Em/G – F – C/E – Dm – G

## Kéo thả MIDI

Giữ chuột vào nút **Kéo hợp âm / Kéo bass / Kéo cả hai** rồi thả vào Piano roll hoặc Playlist của FL. Bấm một lần (không kéo) để lưu file `.mid` vào `Documents\SoulChord`.

Plugin cũng gửi MIDI ra (kênh 1 = hợp âm, kênh 2 = bass). Muốn dùng hợp âm của SoulChord điều khiển synth khác trực tiếp, đặt *MIDI output port* trong cài đặt wrapper của plugin (biểu tượng bánh răng → Settings) trùng với *MIDI input port* của synth đó.

## Build từ code

Mỗi lần push lên nhánh `main`, GitHub Actions tự build bản Windows. Gắn tag `v1.0.0` (hoặc tương tự) để tạo bản phát hành trong Releases.

Build trên máy:

```
cmake -B build -A x64
cmake --build build --config Release
```

Cấu trúc code:
- `Source/Theory.*` – lõi nhạc lý: thang âm, dựng hợp âm, voicing, voice leading, bass, tên hợp âm, ký hiệu La Mã
- `Source/Arranger.*` – thư viện vòng, passing chord, pattern nhịp → chuỗi nốt theo thời gian
- `Source/SynthVoices.h` – tiếng Piano, Rhodes (FM), Pad, Bell, bass sub
- `Source/PluginProcessor.*` – xử lý âm thanh, hai chế độ chơi, xuất MIDI
- `Source/PluginEditor.*` – giao diện
- `tests/` – unit test nhạc lý và test chạy plugin không giao diện

Viết bằng [JUCE](https://juce.com) 8. JUCE dùng giấy phép AGPLv3 / thương mại; bản cá nhân, mã nguồn mở thì dùng miễn phí.
