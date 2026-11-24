#pragma once

#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
#using <System.Windows.Forms.DataVisualization.dll>

using namespace System;
using namespace System::Windows::Forms;
using namespace System::Windows::Forms::DataVisualization::Charting;
using namespace System::Drawing;
using namespace System::Drawing::Drawing2D;
using namespace System::Globalization;
using namespace System::Collections::Generic;

namespace AntennaSimulator {

    public ref class AntennaArray {
    public:
        int Nx, Ny;
        double lambda;
        double beta;
        double d;
        List<PointF>^ elements;
        List<bool>^ activeMask;

        AntennaArray(int nx, int ny, double lam, double b) {
            Nx = Math::Max(1, nx);
            Ny = Math::Max(1, ny);
            lambda = lam;
            beta = b;
            d = beta * lambda / 2.0;

            elements = gcnew List<PointF>();
            activeMask = gcnew List<bool>();

            double centerX = (Nx - 1) * d / 2.0;
            double centerY = (Ny - 1) * d / 2.0;

            for (int i = 0; i < Nx; i++) {
                for (int j = 0; j < Ny; j++) {
                    elements->Add(PointF(
                        (float)(i * d - centerX),
                        (float)(j * d - centerY)
                    ));
                    activeMask->Add(true);
                }
            }
        }

        double CalculateIntensity(double theta_deg, double phi_deg) {
            double theta = theta_deg * Math::PI / 180.0;
            double phi = phi_deg * Math::PI / 180.0;
            double k = 2.0 * Math::PI / lambda;

            double sumRe = 0.0, sumIm = 0.0;
            int active = 0;

            for (int i = 0; i < elements->Count; i++) {
                if (!activeMask[i]) continue;
                active++;

                double x = elements[i].X;
                double y = elements[i].Y;

                double phase = k * (x * Math::Sin(theta) * Math::Cos(phi) +
                    y * Math::Sin(theta) * Math::Sin(phi));

                sumRe += Math::Cos(phase);
                sumIm += Math::Sin(phase);
            }

            if (active == 0) return 0.0;

            double intensity = sumRe * sumRe + sumIm * sumIm;
            return intensity / (active * active);
        }
    };

    public ref class MyForm : public Form {
    private:
        TextBox^ tbNx;
        TextBox^ tbNy;
        TextBox^ tbBeta;
        ComboBox^ cbViewMode;
        Button^ btnCalculate;
        Button^ btnResetMask;
        Button^ btnGrating;
        Chart^ chart2D;
        PictureBox^ pbInteractiveGrid;
        PictureBox^ pbView;

        AntennaArray^ array;

        // ---- 3D-камера ----
        double angleX = 0.85;   // наклон (0 = сверху, π/2 = сбоку)
        double angleY = 0.35;   // поворот
        double zoom3D = 1.0;
        Bitmap^ bmp3D;
        bool isRotating = false;
        Point lastMousePos;
        static const double mouseRotateSpeed = 0.005;

    public:
        MyForm(void) {
            InitializeComponent();
            InitializeArray();
            SetupChart();
            DrawInteractiveGrid();
            OnRecalc(nullptr, nullptr);
        }

    private:
        void InitializeComponent(void) {
            this->Text = L"Симулятор ФАР — Диаграмма направленности";
            this->Size = System::Drawing::Size(1400, 900);
            this->StartPosition = FormStartPosition::CenterScreen;
            this->MinimumSize = System::Drawing::Size(1200, 750);
            this->BackColor = Color::FromArgb(240, 240, 245);
            this->KeyPreview = true;
            this->KeyDown += gcnew KeyEventHandler(this, &MyForm::Form_KeyDown);

            // ====================== ВЕРХНЯЯ ПАНЕЛЬ ======================
            Panel^ pnl = gcnew Panel();
            pnl->Location = Point(12, 12);
            pnl->Size = System::Drawing::Size(1360, 58);
            pnl->BackColor = Color::White;
            pnl->BorderStyle = BorderStyle::FixedSingle;
            this->Controls->Add(pnl);

            Label^ lblNx = gcnew Label();
            lblNx->Text = L"Nx:";
            lblNx->Location = Point(15, 18);
            lblNx->AutoSize = true;
            lblNx->Font = gcnew System::Drawing::Font(L"Segoe UI", 9.5f);
            pnl->Controls->Add(lblNx);

            tbNx = gcnew TextBox();
            tbNx->Text = L"8";
            tbNx->Location = Point(45, 15);
            tbNx->Width = 50;
            tbNx->Font = gcnew System::Drawing::Font(L"Segoe UI", 10);
            pnl->Controls->Add(tbNx);

            Label^ lblNy = gcnew Label();
            lblNy->Text = L"Ny:";
            lblNy->Location = Point(110, 18);
            lblNy->AutoSize = true;
            lblNy->Font = gcnew System::Drawing::Font(L"Segoe UI", 9.5f);
            pnl->Controls->Add(lblNy);

            tbNy = gcnew TextBox();
            tbNy->Text = L"8";
            tbNy->Location = Point(140, 15);
            tbNy->Width = 50;
            tbNy->Font = gcnew System::Drawing::Font(L"Segoe UI", 10);
            pnl->Controls->Add(tbNy);

            Label^ lblBeta = gcnew Label();
            lblBeta->Text = L"beta (d = beta × λ/2):";
            lblBeta->Location = Point(210, 18);
            lblBeta->AutoSize = true;
            lblBeta->Font = gcnew System::Drawing::Font(L"Segoe UI", 9.5f);
            pnl->Controls->Add(lblBeta);

            tbBeta = gcnew TextBox();
            tbBeta->Text = L"1.0";
            tbBeta->Location = Point(365, 15);
            tbBeta->Width = 55;
            tbBeta->Font = gcnew System::Drawing::Font(L"Segoe UI", 10);
            pnl->Controls->Add(tbBeta);

            Label^ lblMode = gcnew Label();
            lblMode->Text = L"Режим:";
            lblMode->Location = Point(445, 18);
            lblMode->AutoSize = true;
            lblMode->Font = gcnew System::Drawing::Font(L"Segoe UI", 9.5f);
            pnl->Controls->Add(lblMode);

            cbViewMode = gcnew ComboBox();
            cbViewMode->Location = Point(505, 14);
            cbViewMode->Width = 260;
            cbViewMode->DropDownStyle = ComboBoxStyle::DropDownList;
            cbViewMode->Font = gcnew System::Drawing::Font(L"Segoe UI", 9.5f);
            cbViewMode->Items->Add(L"3D диаграмма (каркас)");
            cbViewMode->Items->Add(L"Вид сверху (градации серого)");
            cbViewMode->SelectedIndex = 0;
            cbViewMode->SelectedIndexChanged += gcnew EventHandler(this, &MyForm::OnRecalc);
            pnl->Controls->Add(cbViewMode);

            btnCalculate = gcnew Button();
            btnCalculate->Text = L"Пересчитать";
            btnCalculate->Location = Point(790, 12);
            btnCalculate->Size = System::Drawing::Size(115, 32);
            btnCalculate->Font = gcnew System::Drawing::Font(L"Segoe UI", 9.5f);
            btnCalculate->Click += gcnew EventHandler(this, &MyForm::OnRecalc);
            pnl->Controls->Add(btnCalculate);

            btnResetMask = gcnew Button();
            btnResetMask->Text = L"Сбросить маску";
            btnResetMask->Location = Point(915, 12);
            btnResetMask->Size = System::Drawing::Size(125, 32);
            btnResetMask->Font = gcnew System::Drawing::Font(L"Segoe UI", 9.5f);
            btnResetMask->Click += gcnew EventHandler(this, &MyForm::btnResetMask_Click);
            pnl->Controls->Add(btnResetMask);

            btnGrating = gcnew Button();
            btnGrating->Text = L"Grating Lobes (d=λ)";
            btnGrating->Location = Point(1050, 12);
            btnGrating->Size = System::Drawing::Size(150, 32);
            btnGrating->Font = gcnew System::Drawing::Font(L"Segoe UI", 9.5f);
            btnGrating->Click += gcnew EventHandler(this, &MyForm::btnGrating_Click);
            pnl->Controls->Add(btnGrating);

            // ====================== МАСКА ======================
            Label^ lblGrid = gcnew Label();
            lblGrid->Text = L"Маска элементов (клик — вкл/выкл)";
            lblGrid->Location = Point(20, 85);
            lblGrid->AutoSize = true;
            lblGrid->Font = gcnew System::Drawing::Font(L"Segoe UI", 9.5f, FontStyle::Bold);
            this->Controls->Add(lblGrid);

            pbInteractiveGrid = gcnew PictureBox();
            pbInteractiveGrid->Location = Point(20, 110);
            pbInteractiveGrid->Size = System::Drawing::Size(260, 260);
            pbInteractiveGrid->BorderStyle = BorderStyle::FixedSingle;
            pbInteractiveGrid->BackColor = Color::White;
            pbInteractiveGrid->MouseClick += gcnew MouseEventHandler(this, &MyForm::pbGrid_Click);
            this->Controls->Add(pbInteractiveGrid);

            // ====================== ОБЛАСТЬ РИСОВАНИЯ ======================
            chart2D = gcnew Chart();
            chart2D->Location = Point(300, 85);
            chart2D->Size = System::Drawing::Size(1070, 750);
            chart2D->ChartAreas->Add(gcnew ChartArea("main"));
            chart2D->Visible = false;          // больше не используется
            this->Controls->Add(chart2D);

            pbView = gcnew PictureBox();
            pbView->Location = Point(300, 85);
            pbView->Size = System::Drawing::Size(1070, 750);
            pbView->BackColor = Color::White;
            pbView->Visible = true;
            pbView->MouseDown += gcnew MouseEventHandler(this, &MyForm::pbView_MouseDown);
            pbView->MouseUp += gcnew MouseEventHandler(this, &MyForm::pbView_MouseUp);
            pbView->MouseMove += gcnew MouseEventHandler(this, &MyForm::pbView_MouseMove);
            pbView->MouseWheel += gcnew MouseEventHandler(this, &MyForm::pbView_MouseWheel);
            this->Controls->Add(pbView);
        }

        void SetupChart() {
            // оставлен для совместимости, но больше не рисуется
            chart2D->Series->Clear();
        }

        void InitializeArray() {
            int nx = 8, ny = 8;
            double beta = 1.0;
            try {
                nx = Convert::ToInt32(tbNx->Text);
                ny = Convert::ToInt32(tbNy->Text);
                beta = Convert::ToDouble(tbBeta->Text, CultureInfo::InvariantCulture);
            }
            catch (...) {}
            array = gcnew AntennaArray(nx, ny, 1.0, beta);
        }

        void DrawInteractiveGrid() {
            if (array == nullptr) return;

            Bitmap^ bmp = gcnew Bitmap(pbInteractiveGrid->Width, pbInteractiveGrid->Height);
            Graphics^ g = Graphics::FromImage(bmp);
            g->Clear(Color::White);
            g->SmoothingMode = SmoothingMode::AntiAlias;

            int cellW = pbInteractiveGrid->Width / array->Nx;
            int cellH = pbInteractiveGrid->Height / array->Ny;

            for (int i = 0; i < array->Nx; i++) {
                for (int j = 0; j < array->Ny; j++) {
                    int idx = i * array->Ny + j;
                    Rectangle rect(i * cellW + 1, j * cellH + 1, cellW - 2, cellH - 2);

                    if (array->activeMask[idx]) {
                        g->FillRectangle(Brushes::LimeGreen, rect);
                        g->DrawRectangle(Pens::DarkGreen, rect);
                    }
                    else {
                        g->FillRectangle(Brushes::Gainsboro, rect);
                        g->DrawRectangle(Pens::Gray, rect);
                    }
                }
            }
            delete g;
            pbInteractiveGrid->Image = bmp;
        }

        void Draw3DWireframe() {
            chart2D->Visible = false;
            pbView->Visible = true;

            int W = pbView->Width;
            int H = pbView->Height;
            if (W < 4 || H < 4) return;

            const int GN = 90;
            double R = 1.0;

            // --- Правильное объявление двумерного массива ---
            cli::array<double, 2>^ hs = gcnew cli::array<double, 2>(GN, GN);
            double maxI = 0.0;

            // 1. Заполняем интенсивность
            for (int gy = 0; gy < GN; gy++) {
                for (int gx = 0; gx < GN; gx++) {
                    double u = -1.0 + 2.0 * gx / (GN - 1);
                    double v = 1.0 - 2.0 * gy / (GN - 1);
                    double r = Math::Sqrt(u * u + v * v);

                    if (r > 1.0) {
                        hs[gy, gx] = System::Double::NaN;
                        continue;
                    }

                    double theta = r * 90.0;
                    double phi = Math::Atan2(v, u) * 180.0 / Math::PI;
                    if (phi < 0.0) phi += 360.0;

                    double val = array->CalculateIntensity(theta, phi);
                    if (val > maxI) maxI = val;
                    hs[gy, gx] = val;
                }
            }
            if (maxI <= 0.0) maxI = 1.0;

            // 2. Переводим в √(I / Imax)
            for (int gy = 0; gy < GN; gy++) {
                for (int gx = 0; gx < GN; gx++) {
                    if (!System::Double::IsNaN(hs[gy, gx])) {
                        hs[gy, gx] = Math::Sqrt(hs[gy, gx] / maxI);
                    }
                }
            }

            // 3. Параметры камеры
            double ca = Math::Cos(angleY);
            double sa = Math::Sin(angleY);
            double ct = Math::Cos(angleX);
            double st = Math::Sin(angleX);
            double scale = zoom3D * 0.42 * Math::Min(W, H) / R;
            double cx = W / 2.0;
            double cy = H / 2.0 + 0.04 * H;
            double zScale = R * 0.75;

            // 4. Буфер
            if (bmp3D == nullptr || bmp3D->Width != W || bmp3D->Height != H) {
                if (bmp3D != nullptr) delete bmp3D;
                bmp3D = gcnew Bitmap(W, H);
            }

            Graphics^ g = Graphics::FromImage(bmp3D);
            g->SmoothingMode = SmoothingMode::AntiAlias;
            g->Clear(Color::White);

            Pen^ pen = gcnew Pen(Color::Black, 1.0f);

            // 5. Линии вдоль X
            for (int gy = 0; gy < GN; gy++) {
                for (int gx = 0; gx < GN - 1; gx++) {
                    double h1 = hs[gy, gx];
                    double h2 = hs[gy, gx + 1];
                    if (System::Double::IsNaN(h1) || System::Double::IsNaN(h2)) continue;

                    double u1 = -1.0 + 2.0 * gx / (GN - 1);
                    double u2 = -1.0 + 2.0 * (gx + 1) / (GN - 1);
                    double v = 1.0 - 2.0 * gy / (GN - 1);

                    double hAvg = (h1 + h2) * 0.5;
                    hAvg = Math::Max(0.0, Math::Min(1.0, hAvg));
                    int c = (int)Math::Round(155.0 * (1.0 - hAvg));
                    pen->Color = Color::FromArgb(c, c, c);

                    // точка 1
                    double x1 = u1 * R, y1 = v * R, z1 = h1 * zScale;
                    double X1a = x1 * ca - y1 * sa;
                    double Y1a = x1 * sa + y1 * ca;
                    double Y2a = Y1a * ct + z1 * st;
                    PointF p1((float)(cx + X1a * scale), (float)(cy - Y2a * scale));

                    // точка 2
                    double x2 = u2 * R, y2 = v * R, z2 = h2 * zScale;
                    double X1b = x2 * ca - y2 * sa;
                    double Y1b = x2 * sa + y2 * ca;
                    double Y2b = Y1b * ct + z2 * st;
                    PointF p2((float)(cx + X1b * scale), (float)(cy - Y2b * scale));

                    g->DrawLine(pen, p1, p2);
                }
            }

            // 6. Линии вдоль Y
            for (int gx = 0; gx < GN; gx++) {
                for (int gy = 0; gy < GN - 1; gy++) {
                    double h1 = hs[gy, gx];
                    double h2 = hs[gy + 1, gx];
                    if (System::Double::IsNaN(h1) || System::Double::IsNaN(h2)) continue;

                    double u = -1.0 + 2.0 * gx / (GN - 1);
                    double v1 = 1.0 - 2.0 * gy / (GN - 1);
                    double v2 = 1.0 - 2.0 * (gy + 1) / (GN - 1);

                    double hAvg = (h1 + h2) * 0.5;
                    hAvg = Math::Max(0.0, Math::Min(1.0, hAvg));
                    int c = (int)Math::Round(155.0 * (1.0 - hAvg));
                    pen->Color = Color::FromArgb(c, c, c);

                    // точка 1
                    double x1 = u * R, y1 = v1 * R, z1 = h1 * zScale;
                    double X1a = x1 * ca - y1 * sa;
                    double Y1a = x1 * sa + y1 * ca;
                    double Y2a = Y1a * ct + z1 * st;
                    PointF p1((float)(cx + X1a * scale), (float)(cy - Y2a * scale));

                    // точка 2
                    double x2 = u * R, y2 = v2 * R, z2 = h2 * zScale;
                    double X1b = x2 * ca - y2 * sa;
                    double Y1b = x2 * sa + y2 * ca;
                    double Y2b = Y1b * ct + z2 * st;
                    PointF p2((float)(cx + X1b * scale), (float)(cy - Y2b * scale));

                    g->DrawLine(pen, p1, p2);
                }
            }

            // подсказка
            g->DrawString(L"← → ↑ ↓ / ЛКМ — вращение   •   +/− или колёсико — масштаб",
                gcnew System::Drawing::Font(L"Segoe UI", 9.0f),
                Brushes::Black, 10, 10);

            delete pen;
            delete g;

            pbView->Image = bmp3D;
        }

        // ====================== ВИД СВЕРХУ ======================
        void DrawTopView() {
            chart2D->Visible = false;
            pbView->Visible = true;

            int W = pbView->Width;
            int H = pbView->Height;

            int size = Math::Min(W, H) - 40;
            int offsetX = (W - size) / 2;
            int offsetY = (H - size) / 2;

            Bitmap^ bmp = gcnew Bitmap(W, H);
            Graphics^ g = Graphics::FromImage(bmp);
            g->Clear(Color::FromArgb(10, 10, 15));
            g->SmoothingMode = SmoothingMode::AntiAlias;

            for (int py = 0; py < size; py++) {
                for (int px = 0; px < size; px++) {
                    double u = (2.0 * px / (size - 1) - 1.0);
                    double v = (1.0 - 2.0 * py / (size - 1));
                    double r = Math::Sqrt(u * u + v * v);

                    if (r > 1.0) {
                        bmp->SetPixel(offsetX + px, offsetY + py, Color::FromArgb(10, 10, 15));
                        continue;
                    }

                    double theta = r * 90.0;
                    double phi = Math::Atan2(v, u) * 180.0 / Math::PI;
                    if (phi < 0) phi += 360.0;

                    double val = array->CalculateIntensity(theta, phi);
                    int gray = (int)(Math::Pow(val, 0.7) * 255.0);
                    if (gray > 255) gray = 255;
                    if (gray < 0) gray = 0;

                    bmp->SetPixel(offsetX + px, offsetY + py, Color::FromArgb(gray, gray, gray));
                }
            }

            g->DrawEllipse(gcnew Pen(Color::FromArgb(80, 255, 255, 255), 1.5f),
                offsetX, offsetY, size - 1, size - 1);

            g->DrawString(L"Вид сверху  •  центр = зенит  •  край = горизонт",
                gcnew System::Drawing::Font(L"Segoe UI", 11, FontStyle::Regular),
                Brushes::White, 18, 14);

            delete g;
            pbView->Image = bmp;
        }

        // ================================================================
        //  Управление камерой
        // ================================================================
        void pbView_MouseDown(Object^ sender, MouseEventArgs^ e) {
            if (e->Button == System::Windows::Forms::MouseButtons::Left) {
                isRotating = true;
                lastMousePos = e->Location;
                pbView->Focus();
            }
        }

        void pbView_MouseUp(Object^ sender, MouseEventArgs^ e) {
            if (e->Button == System::Windows::Forms::MouseButtons::Left)
                isRotating = false;
        }

        void pbView_MouseMove(Object^ sender, MouseEventArgs^ e) {
            if (!isRotating || cbViewMode->SelectedIndex != 0) return;

            int dx = e->X - lastMousePos.X;
            int dy = e->Y - lastMousePos.Y;
            lastMousePos = e->Location;

            angleY += dx * mouseRotateSpeed;
            angleX += dy * mouseRotateSpeed;

            if (angleX > Math::PI / 2) angleX = Math::PI / 2;
            if (angleX < 0) angleX = 0;

            Draw3DWireframe();
        }

        void pbView_MouseWheel(Object^ sender, MouseEventArgs^ e) {
            if (cbViewMode->SelectedIndex != 0) return;
            zoom3D *= (e->Delta > 0) ? 1.12 : 1.0 / 1.12;
            Draw3DWireframe();
        }

        void Form_KeyDown(Object^ sender, KeyEventArgs^ e) {
            if (cbViewMode->SelectedIndex != 0) return;

            const double step = 0.09;
            bool changed = false;

            switch (e->KeyCode) {
            case Keys::Left:  angleY -= step; changed = true; break;
            case Keys::Right: angleY += step; changed = true; break;
            case Keys::Up:    angleX -= step; changed = true; break;
            case Keys::Down:  angleX += step; changed = true; break;
            case Keys::Add:
            case Keys::Oemplus:    zoom3D *= 1.12; changed = true; break;
            case Keys::Subtract:
            case Keys::OemMinus:   zoom3D /= 1.12; changed = true; break;
            }

            if (changed) {
                if (angleX > Math::PI / 2) angleX = Math::PI / 2;
                if (angleX < 0) angleX = 0;
                e->Handled = true;
                Draw3DWireframe();
            }
        }

        // ================================================================
        void OnRecalc(Object^ sender, EventArgs^ e) {
            try {
                int nx = Convert::ToInt32(tbNx->Text);
                int ny = Convert::ToInt32(tbNy->Text);
                double beta = Convert::ToDouble(tbBeta->Text, CultureInfo::InvariantCulture);

                if (nx != array->Nx || ny != array->Ny || Math::Abs(beta - array->beta) > 1e-6) {
                    array = gcnew AntennaArray(nx, ny, 1.0, beta);
                }

                DrawInteractiveGrid();

                switch (cbViewMode->SelectedIndex) {
                case 0: Draw3DWireframe(); break;
                case 1: DrawTopView();     break;
                }
            }
            catch (Exception^ ex) {
                MessageBox::Show(L"Ошибка: " + ex->Message, L"Ошибка ввода");
            }
        }

        void pbGrid_Click(Object^ sender, MouseEventArgs^ e) {
            if (array == nullptr) return;

            int cellW = Math::Max(1, pbInteractiveGrid->Width / array->Nx);
            int cellH = Math::Max(1, pbInteractiveGrid->Height / array->Ny);

            int i = e->X / cellW;
            int j = e->Y / cellH;

            if (i >= 0 && i < array->Nx && j >= 0 && j < array->Ny) {
                int idx = i * array->Ny + j;
                array->activeMask[idx] = !array->activeMask[idx];
                DrawInteractiveGrid();
                OnRecalc(nullptr, nullptr);
            }
        }

        void btnResetMask_Click(Object^ sender, EventArgs^ e) {
            if (array == nullptr) return;
            for (int i = 0; i < array->activeMask->Count; i++)
                array->activeMask[i] = true;
            DrawInteractiveGrid();
            OnRecalc(nullptr, nullptr);
        }

        void btnGrating_Click(Object^ sender, EventArgs^ e) {
            tbBeta->Text = L"2.0";
            OnRecalc(nullptr, nullptr);
            MessageBox::Show(
                L"beta = 2 → d = λ\n\nПоявились grating lobes.\nХорошо видно и в 3D, и на виде сверху.",
                L"Grating Lobes",
                MessageBoxButtons::OK,
                MessageBoxIcon::Information);
        }
    };
}