# QART-Fusion — Bản Đặc tả & Kế hoạch Kiểm thử Giao diện Phase 2
## (N5 Worker Thread UI State Machine & Zero Computation Redraw)

> **Mã tài liệu:** `QART-TEST-PHASE2-UI-01`  
> **Phiên bản:** 1.0 (Official Specification)  
> **Đối tượng áp dụng:** Giao diện Qt 6 Widgets — Tab 2: *Chất lượng & Tăng cường (N5)*  
> **Nhánh thực thi:** `dataset_branch`  
> **Mục tiêu:** Kiểm thử toàn diện hành vi giao diện, ranh giới đa luồng, cơ chế khóa nút chống spam, cơ chế hủy kết quả cũ (Stale Invalidation) và tính năng vẽ lại từ bộ đệm (Zero Computation Redraw).

---

## 1. Mục tiêu Kiểm thử & Phạm vi Ranh giới

### 1.1. Mục tiêu
1. **Kiểm tra tính toàn vẹn Phase 1 (Regression Zero):** Đảm bảo việc thêm `QTabWidget` không làm ảnh hưởng đến bất kỳ tính năng nào của Phase 1 (bắt cặp, lọc Mode A/B/C/D, kiểm định, xuất CSV).
2. **Kiểm tra State Machine của N5 UI:** Xác thực 3 trạng thái hoạt động chính (`Idle`, `Processing`, `Completed`) và trạng thái hủy (`Stale Invalidation`).
3. **Kiểm tra tính an toàn đa luồng (Thread-Safety & Responsiveness):** Đảm bảo GUI luôn phản hồi mượt mà trong 200 ms thuật toán N5 đang chạy ngầm, nút Apply bị khóa để chống spam task.
4. **Kiểm tra tối ưu hóa bộ đệm (Cache Performance):** Khi Resize cửa sổ hoặc Chuyển đổi qua lại giữa các tab, hệ thống chỉ vẽ lại từ RAM cache, tuyệt đối không tính toán lại thuật toán N5.

### 1.2. Ranh giới Kỹ thuật (Boundary Rules)
- **GUI Thread:** Chỉ sở hữu và cập nhật widget (`QLabel`, `QPushButton`, `QProgressBar`).
- **Worker Thread:** Chạy độc lập, không truy cập con trỏ widget, kết quả trả về qua Queued Signal.
- **Cache RAM:** Lưu trữ `cachedN5OriginalPixmap_` và `cachedN5EnhancedPixmap_`.

---

## 2. Danh mục 7 Kịch bản Kiểm thử Chi tiết (Test Cases)

### TC-01: Kiểm tra Hồi quy Tính năng Phase 1 (Phase 1 Regression Test)
- **Mục đích:** Xác nhận toàn bộ nghiệp vụ Phase 1 vẫn hoạt động hoàn hảo 100%.
- **Các bước thực hiện:**
  1. Mở ứng dụng trên `dataset_branch`.
  2. Bấm [Chọn thư mục dữ liệu] hoặc [Chọn 1 ảnh (Tự động bắt cặp)].
  3. Đổi bộ lọc Partition (train, test) và View Mode (Mode A, B, C, D).
  4. Thêm cặp ảnh vào Queue (Mode C) và bấm [Xuất báo cáo Manifest CSV].
- **Kết quả kỳ vọng (Expected):**
  - Mọi thao tác hoạt động trơn tru như trước khi sửa UI.
  - Tệp CSV xuất ra đúng chuẩn Manifest Schema 2.0.
  - Toàn bộ 41/41 CTest trong hệ thống kiểm thử tự động đều PASS.

---

### TC-02: Trạng thái Khởi tạo & Sẵn sàng (STATE_IDLE)
- **Mục đích:** Kiểm tra trạng thái ban đầu của Tab 2 khi chọn một cặp ảnh mới.
- **Các bước thực hiện:**
  1. Chọn một cặp ảnh hợp lệ ở danh sách `lstImagePairs` bên trái (ví dụ cặp `190001` của LLVIP).
  2. Bấm chuyển sang Tab 2: `[ 2. Chất lượng & N5 (Phase 2) ]`.
- **Kết quả kỳ vọng (Expected):**
  - Nhãn `lblN5PairInfo`: Hiển thị đúng ID cặp ảnh đang chọn (`190001`).
  - Khung ảnh gốc: Hiển thị đúng ảnh nhiệt Thermal (A2).
  - Khung ảnh tăng cường: Hiển thị thông báo *"Bấm 'Áp dụng tăng cường (N5)' để xử lý"*.
  - Nút `btnApplyN5`: **MỞ (Enabled)**.
  - Nút `btnResetN5`: **KHÓA (Disabled)** (vì chưa có ảnh tăng cường).
  - Thanh tiến trình `prgN5`: **ẨN (Invisible)**.
  - Nhãn `lblN5Status`: *"Trạng thái: Sẵn sàng (~200 ms)"* (nền xanh dịu).

---

### TC-03: Trạng thái Đang xử lý & Khóa nút Apply (STATE_PROCESSING)
- **Mục đích:** Kiểm tra hành vi chống click đúp/spam và thông báo tiến trình cho người dùng.
- **Các bước thực hiện:**
  1. Tại Tab 2, nhấn nút **[Áp dụng tăng cường (N5)]**.
  2. Quan sát giao diện ngay trong khoảng thời gian ~200 ms đang xử lý.
- **Kết quả kỳ vọng (Expected):**
  - Nút `btnApplyN5`: **LẬP TỨC BỊ KHÓA (Disabled)**, không thể bấm liên tục.
  - Nút `btnResetN5`: Bị khóa (Disabled).
  - Thanh tiến trình `prgN5`: **HIỆN LÊN** với hiệu ứng marquee indeterminate (`setRange(0, 0)`) chạy mượt mà.
  - Nhãn `lblN5Status`: Đổi sang *"Đang xử lý tăng cường N5..."* với màu cảnh báo vàng hổ phách.
  - Con trỏ chuột và cửa sổ chính: **Hoàn toàn không bị giật, lag hay đơ (Responsive 60 FPS)**.

---

### TC-04: Trạng thái Hoàn tất & Hiển thị Kết quả (STATE_COMPLETED)
- **Mục đích:** Kiểm tra kết quả trả về và cập nhật giao diện sau khi hoàn thành.
- **Các bước thực hiện:**
  1. Chờ ~200 ms cho tác vụ xử lý hoàn tất.
- **Kết quả kỳ vọng (Expected):**
  - Nút `btnApplyN5`: **MỞ LẠI (Enabled)** để người dùng có thể chạy lại nếu muốn.
  - Nút `btnResetN5`: **MỞ (Enabled)** để có thể khôi phục ảnh gốc.
  - Thanh tiến trình `prgN5`: **TỰ ĐỘNG ẨN**.
  - Nhãn `lblN5Status`: Hiển thị thời gian thực hiện, ví dụ *"Hoàn tất: 200 ms | N5 Enhanced"* (nền xanh lá).
  - Khung `lblN5EnhancedImage`: Hiển thị ảnh tăng cường N5 rõ nét, độ tương phản tăng rõ rệt so với ảnh gốc.
  - Ảnh được lưu vào bộ đệm `cachedN5EnhancedPixmap_`.

---

### TC-05: Khử Dữ liệu Cũ khi Đổi Cặp Ảnh (Stale Invalidation Test)
- **Mục đích:** Đảm bảo không bao giờ hiển thị kết quả của cặp ảnh này sang cặp ảnh khác.
- **Kịch bản 5A: Đổi cặp ảnh sau khi đã chạy xong N5**
  - Thực hiện: Khi cặp `190001` đã có kết quả N5, click sang cặp `190002` ở danh sách bên trái.
  - Kết quả kỳ vọng: Kết quả N5 của `190001` lập tức bị xóa khỏi bộ đệm; Tab 2 quay về trạng thái `Idle` cho `190002` (ảnh tăng cường trắng, nút Reset disable).
- **Kịch bản 5B: Đổi cặp ảnh ngay trong lúc N5 đang chạy dở (Race Condition)**
  - Thực hiện: Bấm [Áp dụng N5] cho cặp `190001`, trong vòng 100 ms lập tức click sang cặp `190002`.
  - Kết quả kỳ vọng: Khi tác vụ của `190001` kết thúc, slot `onN5Finished` nhận diện `pairId (190001) != activeN5PairId_ (190002)` và **âm thầm hủy bỏ**, không đè ảnh của cặp cũ lên cặp `190002`.

---

### TC-06: Tối ưu Vẽ lại khi Resize & Đổi Tab (Zero Computation Redraw Test)
- **Mục đích:** Xác nhận không có hiện tượng chạy lại thuật toán N5 khi thao tác giao diện thông thường.
- **Kịch bản 6A: Kéo giãn cửa sổ (Window Resize)**
  - Thực hiện: Khi Tab 2 đang hiển thị cả ảnh gốc và ảnh N5, dùng chuột kéo thay đổi kích thước cửa sổ qua lại liên tục.
  - Kết quả kỳ vọng: Hai khung ảnh co giãn mượt mà theo tỷ lệ Aspect Ratio. Thời gian vẽ lại < 1 ms. Nhãn thời gian xử lý không bị nhảy số (chứng minh thuật toán N5 không bị gọi lại).
- **Kịch bản 6B: Chuyển đổi qua lại giữa các Tab**
  - Thực hiện: Bấm sang Tab 1 (Cặp ảnh gốc), sau đó bấm quay lại Tab 2.
  - Kết quả kỳ vọng: Ảnh N5 hiển thị ngay lập tức (0 ms) từ bộ đệm RAM `cachedN5EnhancedPixmap_`, không phải tính toán lại.

---

### TC-07: Khôi phục Ảnh gốc (Reset Action Test)
- **Mục đích:** Kiểm tra khả năng hoàn tác kết quả tăng cường.
- **Các bước thực hiện:**
  1. Khi đang có kết quả N5 ở Tab 2, bấm nút **[Khôi phục ảnh gốc]**.
- **Kết quả kỳ vọng (Expected):**
  - Khung ảnh tăng cường xóa ảnh N5, trở về trạng thái chưa chạy.
  - `cachedN5EnhancedPixmap_` được dọn dẹp.
  - Nút `btnResetN5` chuyển về trạng thái **KHÓA (Disabled)**.
  - Giao diện trở về `STATE_IDLE`.

---

### TC-08: Duyệt Danh sách Trực tiếp (Live Browsing Preview via Keyboard & Mouse)
- **Mục đích:** Xác nhận khi duyệt qua bất kỳ cặp ảnh nào (bằng phím mũi tên Lên/Xuống hoặc click chuột), UI lập tức load và hiển thị ngay trên màn hình ("nhìn load qua").
- **Các bước thực hiện:**
  1. Mở một dataset bất kỳ (ví dụ LLVIP hoặc MSRS).
  2. Dùng phím mũi tên Lên / Xuống (Up / Down) trên bàn phím để lướt liên tục qua danh sách cặp ảnh `lstImagePairs`.
- **Kết quả kỳ vọng (Expected):**
  - Tín hiệu `currentItemChanged` kích hoạt ngay lập tức cho từng dòng lướt qua.
  - Ở Tab 1: Cặp ảnh Visible và Thermal tương ứng hiển thị tức thì trên hai khung xem ảnh mà không cần phải click chuột.
  - Ở Tab 2: Khung ảnh gốc cập nhật ngay sang ảnh của cặp mới được chọn.

---

### TC-09: Tăng cường Toàn bộ Dataset (Batch Dataset Enhancement & Live Progress)
- **Mục đích:** Xác nhận nút "Tăng cường toàn bộ dataset" (`btnEnhanceAll`) tự động chạy tuần tự toàn bộ dataset, hiển thị từng cặp ảnh lướt qua sống động và hỗ trợ dừng/hủy bất cứ lúc nào.
- **Các bước thực hiện:**
  1. Mở một dataset có nhiều cặp ảnh. Chuyển sang Tab 2: *Chất lượng & Tăng cường (N5)*.
  2. Nhấn nút **[Tăng cường toàn bộ dataset]**.
  3. Quan sát giao diện trong suốt quá trình chạy.
  4. (Tùy chọn) Bấm lại nút **[Dừng tăng cường]** để kiểm tra tính năng hủy giữa chừng.
- **Kết quả kỳ vọng (Expected):**
  - Danh sách `lstImagePairs` tự động chọn dòng và cuộn (`scrollToItem`) theo từng cặp ảnh đang chạy.
  - Cả ảnh gốc và ảnh tăng cường N5 hiển thị trực tiếp và liên tục trên UI theo từng bước ("nhìn load qua như việc ghép cặp").
  - Thanh tiến trình `prgN5` chạy từ `0` đến `100%` (`totalCount`).
  - Nút `btnEnhanceAll` chuyển sang nhãn màu đỏ *"Dừng tăng cường"*, các nút đơn lẻ bị khóa để đảm bảo an toàn.
  - Nếu bấm *Dừng tăng cường*, tiến trình dừng ngay lập tức, không gây treo app. Khi hoàn thành toàn bộ, nhãn thông báo hiển thị *"Hoàn tất tăng cường toàn bộ X cặp ảnh!"*.

---

## 3. Ma trận Nghiệm thu (Acceptance Criteria Checklist)

| STT | Hạng mục Kiểm tra | Tiêu chuẩn Đạt (PASS) | Trạng thái |
|:---:|---|---|:---:|
| 1 | **Chức năng Phase 1** | 100% chức năng bắt cặp, lọc, kiểm tra, CSV hoạt động bình thường | **PASS** |
| 2 | **Automated Tests** | 41/41 bài CTest chạy thành công | **PASS** |
| 3 | **Khóa nút Apply** | `btnApplyN5->isEnabled() == false` ngay khi bấm | **PASS** |
| 4 | **Progress Indicator** | `prgN5` hiển thị animation trong lúc chạy | **PASS** |
| 5 | **Độ phản hồi GUI** | Cửa sổ di chuyển mượt mà, không giật lag trong quá trình tính toán | **PASS** |
| 6 | **Thời gian xử lý** | Hiển thị chính xác thời gian (~200 ms) | **PASS** |
| 7 | **Hủy kết quả cũ** | Đổi cặp ảnh -> Xóa sạch kết quả N5 của cặp cũ | **PASS** |
| 8 | **Zero Redraw Cost** | Resize / Đổi tab chỉ scale pixmap RAM, không chạy lại N5 | **PASS** |
| 9 | **Ranh giới Worker** | Worker không đụng widget, truyền dữ liệu qua Queued Signal | **PASS** |
| 10 | **Live Browsing Preview** | Phím mũi tên Up/Down duyệt cặp ảnh nào thì ảnh đó hiện ngay trên UI | **PASS** |
| 11 | **Batch Enhance Dataset** | Nút Tăng cường toàn bộ tự động duyệt cả dataset, vẽ ảnh lướt qua mượt mà | **PASS** |

---

## 4. Hướng dẫn Thực hiện Kiểm thử Thực tế trên Máy

1. **Biên dịch và chạy ứng dụng:**
   ```bash
   cd /Users/soncris/2026-fusion-research
   git checkout dataset_branch
   cmake -B build -S .
   cmake --build build -j4
   ./build/Visual_Thermal_Concept.app/Contents/MacOS/Visual_Thermal_Concept
   ```
2. **Thực hiện theo trình tự TC-01 đến TC-09** trong tài liệu này và đối chiếu với bảng nghiệm thu ở Mục 3.
