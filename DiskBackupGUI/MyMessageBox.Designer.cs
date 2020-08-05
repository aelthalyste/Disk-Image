namespace DiskBackupGUI
{
    partial class MyMessageBox
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MyMessageBox));
            this.panelTop = new System.Windows.Forms.Panel();
            this.pictureBox1 = new System.Windows.Forms.PictureBox();
            this.btnOkay = new FontAwesome.Sharp.IconButton();
            this.panelBottom = new System.Windows.Forms.Panel();
            this.bunifuElipse1 = new Bunifu.Framework.UI.BunifuElipse(this.components);
            this.datePicker = new System.Windows.Forms.DateTimePicker();
            this.timepicker = new System.Windows.Forms.DateTimePicker();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.panel1 = new System.Windows.Forms.Panel();
            this.bunifuSeparator4 = new Bunifu.Framework.UI.BunifuSeparator();
            this.label5 = new System.Windows.Forms.Label();
            this.btnNowOkay = new FontAwesome.Sharp.IconButton();
            this.panel2 = new System.Windows.Forms.Panel();
            this.btnRepeatOkay = new FontAwesome.Sharp.IconButton();
            this.label10 = new System.Windows.Forms.Label();
            this.label9 = new System.Windows.Forms.Label();
            this.txtRepeatCount = new System.Windows.Forms.TextBox();
            this.label8 = new System.Windows.Forms.Label();
            this.txtRepeatTime = new System.Windows.Forms.TextBox();
            this.label7 = new System.Windows.Forms.Label();
            this.bunifuSeparator3 = new Bunifu.Framework.UI.BunifuSeparator();
            this.label6 = new System.Windows.Forms.Label();
            this.repeatTimePicker = new System.Windows.Forms.DateTimePicker();
            this.label4 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.repeatDatePicker = new System.Windows.Forms.DateTimePicker();
            this.panelTop.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).BeginInit();
            this.panel1.SuspendLayout();
            this.panel2.SuspendLayout();
            this.SuspendLayout();
            // 
            // panelTop
            // 
            this.panelTop.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.panelTop.Controls.Add(this.pictureBox1);
            this.panelTop.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelTop.Location = new System.Drawing.Point(0, 0);
            this.panelTop.Name = "panelTop";
            this.panelTop.Size = new System.Drawing.Size(660, 58);
            this.panelTop.TabIndex = 8;
            this.panelTop.MouseDown += new System.Windows.Forms.MouseEventHandler(this.panelTop_MouseDown);
            // 
            // pictureBox1
            // 
            this.pictureBox1.Dock = System.Windows.Forms.DockStyle.Left;
            this.pictureBox1.Image = ((System.Drawing.Image)(resources.GetObject("pictureBox1.Image")));
            this.pictureBox1.Location = new System.Drawing.Point(0, 0);
            this.pictureBox1.Name = "pictureBox1";
            this.pictureBox1.Size = new System.Drawing.Size(126, 58);
            this.pictureBox1.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.pictureBox1.TabIndex = 0;
            this.pictureBox1.TabStop = false;
            // 
            // btnOkay
            // 
            this.btnOkay.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.btnOkay.FlatAppearance.BorderSize = 0;
            this.btnOkay.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnOkay.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnOkay.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnOkay.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnOkay.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.btnOkay.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnOkay.IconSize = 32;
            this.btnOkay.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnOkay.Location = new System.Drawing.Point(38, 174);
            this.btnOkay.Name = "btnOkay";
            this.btnOkay.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.btnOkay.Rotation = 0D;
            this.btnOkay.Size = new System.Drawing.Size(186, 60);
            this.btnOkay.TabIndex = 7;
            this.btnOkay.Text = "Görev Oluştur";
            this.btnOkay.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnOkay.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnOkay.UseVisualStyleBackColor = false;
            this.btnOkay.Click += new System.EventHandler(this.btnOkay_Click);
            // 
            // panelBottom
            // 
            this.panelBottom.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.panelBottom.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.panelBottom.Location = new System.Drawing.Point(0, 298);
            this.panelBottom.Name = "panelBottom";
            this.panelBottom.Size = new System.Drawing.Size(660, 10);
            this.panelBottom.TabIndex = 5;
            // 
            // bunifuElipse1
            // 
            this.bunifuElipse1.ElipseRadius = 50;
            this.bunifuElipse1.TargetControl = this;
            // 
            // datePicker
            // 
            this.datePicker.Location = new System.Drawing.Point(136, 42);
            this.datePicker.Name = "datePicker";
            this.datePicker.Size = new System.Drawing.Size(158, 20);
            this.datePicker.TabIndex = 10;
            // 
            // timepicker
            // 
            this.timepicker.CustomFormat = "hh:mm";
            this.timepicker.Format = System.Windows.Forms.DateTimePickerFormat.Time;
            this.timepicker.Location = new System.Drawing.Point(138, 83);
            this.timepicker.Name = "timepicker";
            this.timepicker.ShowUpDown = true;
            this.timepicker.Size = new System.Drawing.Size(155, 20);
            this.timepicker.TabIndex = 11;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F);
            this.label1.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.label1.Location = new System.Drawing.Point(10, 42);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(120, 20);
            this.label1.TabIndex = 12;
            this.label1.Text = "Başlangıç Tarihi";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F);
            this.label2.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.label2.Location = new System.Drawing.Point(11, 84);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(119, 20);
            this.label2.TabIndex = 13;
            this.label2.Text = "Başlangıç Saati";
            // 
            // panel1
            // 
            this.panel1.Controls.Add(this.bunifuSeparator4);
            this.panel1.Controls.Add(this.label5);
            this.panel1.Controls.Add(this.btnOkay);
            this.panel1.Controls.Add(this.timepicker);
            this.panel1.Controls.Add(this.label2);
            this.panel1.Controls.Add(this.datePicker);
            this.panel1.Controls.Add(this.label1);
            this.panel1.Dock = System.Windows.Forms.DockStyle.Left;
            this.panel1.Location = new System.Drawing.Point(0, 58);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(294, 240);
            this.panel1.TabIndex = 14;
            this.panel1.MouseDown += new System.Windows.Forms.MouseEventHandler(this.panel1_MouseDown);
            // 
            // bunifuSeparator4
            // 
            this.bunifuSeparator4.BackColor = System.Drawing.Color.Transparent;
            this.bunifuSeparator4.LineColor = System.Drawing.Color.FromArgb(((int)(((byte)(105)))), ((int)(((byte)(105)))), ((int)(((byte)(105)))));
            this.bunifuSeparator4.LineThickness = 1;
            this.bunifuSeparator4.Location = new System.Drawing.Point(-2, 26);
            this.bunifuSeparator4.Name = "bunifuSeparator4";
            this.bunifuSeparator4.Size = new System.Drawing.Size(286, 10);
            this.bunifuSeparator4.TabIndex = 18;
            this.bunifuSeparator4.Transparency = 255;
            this.bunifuSeparator4.Vertical = false;
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F);
            this.label5.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.label5.Location = new System.Drawing.Point(92, 3);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(107, 20);
            this.label5.TabIndex = 14;
            this.label5.Text = "Görev Oluştur";
            // 
            // btnNowOkay
            // 
            this.btnNowOkay.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.btnNowOkay.FlatAppearance.BorderSize = 0;
            this.btnNowOkay.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnNowOkay.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnNowOkay.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnNowOkay.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnNowOkay.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.btnNowOkay.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnNowOkay.IconSize = 32;
            this.btnNowOkay.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnNowOkay.Location = new System.Drawing.Point(230, 235);
            this.btnNowOkay.Name = "btnNowOkay";
            this.btnNowOkay.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.btnNowOkay.Rotation = 0D;
            this.btnNowOkay.Size = new System.Drawing.Size(210, 60);
            this.btnNowOkay.TabIndex = 25;
            this.btnNowOkay.Text = "Şimdi Çalıştır";
            this.btnNowOkay.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnNowOkay.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnNowOkay.UseVisualStyleBackColor = false;
            this.btnNowOkay.Click += new System.EventHandler(this.btnNowOkay_Click);
            // 
            // panel2
            // 
            this.panel2.Controls.Add(this.btnRepeatOkay);
            this.panel2.Controls.Add(this.label10);
            this.panel2.Controls.Add(this.label9);
            this.panel2.Controls.Add(this.txtRepeatCount);
            this.panel2.Controls.Add(this.label8);
            this.panel2.Controls.Add(this.txtRepeatTime);
            this.panel2.Controls.Add(this.label7);
            this.panel2.Controls.Add(this.bunifuSeparator3);
            this.panel2.Controls.Add(this.label6);
            this.panel2.Controls.Add(this.repeatTimePicker);
            this.panel2.Controls.Add(this.label4);
            this.panel2.Controls.Add(this.label3);
            this.panel2.Controls.Add(this.repeatDatePicker);
            this.panel2.Dock = System.Windows.Forms.DockStyle.Right;
            this.panel2.Location = new System.Drawing.Point(356, 58);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(304, 240);
            this.panel2.TabIndex = 15;
            this.panel2.MouseDown += new System.Windows.Forms.MouseEventHandler(this.panel2_MouseDown);
            // 
            // btnRepeatOkay
            // 
            this.btnRepeatOkay.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.btnRepeatOkay.FlatAppearance.BorderSize = 0;
            this.btnRepeatOkay.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnRepeatOkay.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnRepeatOkay.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnRepeatOkay.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnRepeatOkay.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.btnRepeatOkay.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnRepeatOkay.IconSize = 32;
            this.btnRepeatOkay.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnRepeatOkay.Location = new System.Drawing.Point(66, 180);
            this.btnRepeatOkay.Name = "btnRepeatOkay";
            this.btnRepeatOkay.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.btnRepeatOkay.Rotation = 0D;
            this.btnRepeatOkay.Size = new System.Drawing.Size(210, 60);
            this.btnRepeatOkay.TabIndex = 19;
            this.btnRepeatOkay.Text = "T. Görev Oluştur";
            this.btnRepeatOkay.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnRepeatOkay.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnRepeatOkay.UseVisualStyleBackColor = false;
            this.btnRepeatOkay.Click += new System.EventHandler(this.btnRepeatOkay_Click);
            // 
            // label10
            // 
            this.label10.AutoSize = true;
            this.label10.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F);
            this.label10.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.label10.Location = new System.Drawing.Point(260, 148);
            this.label10.Name = "label10";
            this.label10.Size = new System.Drawing.Size(41, 20);
            this.label10.TabIndex = 24;
            this.label10.Text = "defa";
            // 
            // label9
            // 
            this.label9.AutoSize = true;
            this.label9.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F);
            this.label9.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.label9.Location = new System.Drawing.Point(274, 117);
            this.label9.Name = "label9";
            this.label9.Size = new System.Drawing.Size(30, 20);
            this.label9.TabIndex = 23;
            this.label9.Text = "dk.";
            // 
            // txtRepeatCount
            // 
            this.txtRepeatCount.Location = new System.Drawing.Point(146, 148);
            this.txtRepeatCount.Name = "txtRepeatCount";
            this.txtRepeatCount.Size = new System.Drawing.Size(109, 20);
            this.txtRepeatCount.TabIndex = 22;
            // 
            // label8
            // 
            this.label8.AutoSize = true;
            this.label8.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F);
            this.label8.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.label8.Location = new System.Drawing.Point(-2, 151);
            this.label8.Name = "label8";
            this.label8.Size = new System.Drawing.Size(99, 20);
            this.label8.TabIndex = 21;
            this.label8.Text = "Tekrar Sayısı";
            // 
            // txtRepeatTime
            // 
            this.txtRepeatTime.Location = new System.Drawing.Point(146, 116);
            this.txtRepeatTime.Name = "txtRepeatTime";
            this.txtRepeatTime.Size = new System.Drawing.Size(109, 20);
            this.txtRepeatTime.TabIndex = 20;
            // 
            // label7
            // 
            this.label7.AutoSize = true;
            this.label7.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F);
            this.label7.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.label7.Location = new System.Drawing.Point(-2, 117);
            this.label7.Name = "label7";
            this.label7.Size = new System.Drawing.Size(141, 20);
            this.label7.TabIndex = 19;
            this.label7.Text = "Tekrarlanıcak Süre";
            // 
            // bunifuSeparator3
            // 
            this.bunifuSeparator3.BackColor = System.Drawing.Color.Transparent;
            this.bunifuSeparator3.LineColor = System.Drawing.Color.FromArgb(((int)(((byte)(105)))), ((int)(((byte)(105)))), ((int)(((byte)(105)))));
            this.bunifuSeparator3.LineThickness = 1;
            this.bunifuSeparator3.Location = new System.Drawing.Point(1, 26);
            this.bunifuSeparator3.Name = "bunifuSeparator3";
            this.bunifuSeparator3.Size = new System.Drawing.Size(303, 10);
            this.bunifuSeparator3.TabIndex = 17;
            this.bunifuSeparator3.Transparency = 255;
            this.bunifuSeparator3.Vertical = false;
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F);
            this.label6.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.label6.Location = new System.Drawing.Point(52, 3);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(193, 20);
            this.label6.TabIndex = 15;
            this.label6.Text = "Tekrarlayan Görev Oluştur";
            // 
            // repeatTimePicker
            // 
            this.repeatTimePicker.CustomFormat = "hh:mm";
            this.repeatTimePicker.Format = System.Windows.Forms.DateTimePickerFormat.Time;
            this.repeatTimePicker.Location = new System.Drawing.Point(146, 82);
            this.repeatTimePicker.Name = "repeatTimePicker";
            this.repeatTimePicker.ShowUpDown = true;
            this.repeatTimePicker.Size = new System.Drawing.Size(155, 20);
            this.repeatTimePicker.TabIndex = 15;
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F);
            this.label4.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.label4.Location = new System.Drawing.Point(-3, 42);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(120, 20);
            this.label4.TabIndex = 16;
            this.label4.Text = "Başlangıç Tarihi";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F);
            this.label3.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.label3.Location = new System.Drawing.Point(-4, 83);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(119, 20);
            this.label3.TabIndex = 17;
            this.label3.Text = "Başlangıç Saati";
            // 
            // repeatDatePicker
            // 
            this.repeatDatePicker.Location = new System.Drawing.Point(146, 42);
            this.repeatDatePicker.Name = "repeatDatePicker";
            this.repeatDatePicker.Size = new System.Drawing.Size(158, 20);
            this.repeatDatePicker.TabIndex = 14;
            // 
            // MyMessageBox
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.ClientSize = new System.Drawing.Size(660, 308);
            this.Controls.Add(this.btnNowOkay);
            this.Controls.Add(this.panel2);
            this.Controls.Add(this.panel1);
            this.Controls.Add(this.panelTop);
            this.Controls.Add(this.panelBottom);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.None;
            this.Name = "MyMessageBox";
            this.Opacity = 0.96D;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Form2";
            this.MouseDown += new System.Windows.Forms.MouseEventHandler(this.MyMessageBox_MouseDown);
            this.panelTop.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).EndInit();
            this.panel1.ResumeLayout(false);
            this.panel1.PerformLayout();
            this.panel2.ResumeLayout(false);
            this.panel2.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Panel panelTop;
        private FontAwesome.Sharp.IconButton btnOkay;
        private System.Windows.Forms.Panel panelBottom;
        private Bunifu.Framework.UI.BunifuElipse bunifuElipse1;
        private System.Windows.Forms.PictureBox pictureBox1;
        private System.Windows.Forms.DateTimePicker timepicker;
        private System.Windows.Forms.DateTimePicker datePicker;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.Panel panel2;
        private System.Windows.Forms.DateTimePicker repeatTimePicker;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.DateTimePicker repeatDatePicker;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.Label label6;
        private Bunifu.Framework.UI.BunifuSeparator bunifuSeparator3;
        private Bunifu.Framework.UI.BunifuSeparator bunifuSeparator4;
        private System.Windows.Forms.Label label7;
        private System.Windows.Forms.TextBox txtRepeatTime;
        private FontAwesome.Sharp.IconButton btnRepeatOkay;
        private System.Windows.Forms.TextBox txtRepeatCount;
        private System.Windows.Forms.Label label8;
        private System.Windows.Forms.Label label10;
        private System.Windows.Forms.Label label9;
        private FontAwesome.Sharp.IconButton btnNowOkay;
    }
}