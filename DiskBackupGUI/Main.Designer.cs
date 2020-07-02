namespace DiskBackupGUI
{
    partial class Main
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Main));
            this.dataGridView1 = new System.Windows.Forms.DataGridView();
            this.folderBrowserDialog1 = new System.Windows.Forms.FolderBrowserDialog();
            this.rtReport = new System.Windows.Forms.RichTextBox();
            this.btnAdd = new FontAwesome.Sharp.IconButton();
            this.btnPath = new FontAwesome.Sharp.IconButton();
            this.btnRestore = new FontAwesome.Sharp.IconButton();
            this.panelMenu = new System.Windows.Forms.Panel();
            this.panel5 = new System.Windows.Forms.Panel();
            this.panelPictures = new System.Windows.Forms.Panel();
            this.pictureBox1 = new System.Windows.Forms.PictureBox();
            this.panelTitleBar = new System.Windows.Forms.Panel();
            this.btnMinimize = new FontAwesome.Sharp.IconPictureBox();
            this.btnExit = new FontAwesome.Sharp.IconPictureBox();
            this.bunifuElipse1 = new Bunifu.Framework.UI.BunifuElipse(this.components);
            this.panelMain = new System.Windows.Forms.Panel();
            this.bunifuElipse3 = new Bunifu.Framework.UI.BunifuElipse(this.components);
            this.panelBottomBar = new System.Windows.Forms.Panel();
            this.btnDifferential = new FontAwesome.Sharp.IconButton();
            this.btnIncremental = new FontAwesome.Sharp.IconButton();
            this.btnCancel = new FontAwesome.Sharp.IconButton();
            ((System.ComponentModel.ISupportInitialize)(this.dataGridView1)).BeginInit();
            this.panelMenu.SuspendLayout();
            this.panel5.SuspendLayout();
            this.panelPictures.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).BeginInit();
            this.panelTitleBar.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.btnMinimize)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.btnExit)).BeginInit();
            this.panelMain.SuspendLayout();
            this.panelBottomBar.SuspendLayout();
            this.SuspendLayout();
            // 
            // dataGridView1
            // 
            this.dataGridView1.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dataGridView1.Location = new System.Drawing.Point(30, 25);
            this.dataGridView1.Name = "dataGridView1";
            this.dataGridView1.Size = new System.Drawing.Size(904, 166);
            this.dataGridView1.TabIndex = 10;
            this.dataGridView1.CellClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.dataGridView1_CellClick);
            // 
            // rtReport
            // 
            this.rtReport.BackColor = System.Drawing.Color.White;
            this.rtReport.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.rtReport.Dock = System.Windows.Forms.DockStyle.Fill;
            this.rtReport.Font = new System.Drawing.Font("Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.rtReport.ForeColor = System.Drawing.Color.Black;
            this.rtReport.Location = new System.Drawing.Point(0, 0);
            this.rtReport.Name = "rtReport";
            this.rtReport.ReadOnly = true;
            this.rtReport.Size = new System.Drawing.Size(967, 129);
            this.rtReport.TabIndex = 0;
            this.rtReport.Text = "";
            // 
            // btnAdd
            // 
            this.btnAdd.Dock = System.Windows.Forms.DockStyle.Top;
            this.btnAdd.FlatAppearance.BorderSize = 0;
            this.btnAdd.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnAdd.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnAdd.Font = new System.Drawing.Font("Microsoft Sans Serif", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnAdd.ForeColor = System.Drawing.Color.Gainsboro;
            this.btnAdd.IconChar = FontAwesome.Sharp.IconChar.PlusCircle;
            this.btnAdd.IconColor = System.Drawing.Color.White;
            this.btnAdd.IconSize = 32;
            this.btnAdd.Location = new System.Drawing.Point(0, 0);
            this.btnAdd.Name = "btnAdd";
            this.btnAdd.Rotation = 0D;
            this.btnAdd.Size = new System.Drawing.Size(151, 60);
            this.btnAdd.TabIndex = 19;
            this.btnAdd.Text = "Add";
            this.btnAdd.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnAdd.UseVisualStyleBackColor = true;
            this.btnAdd.Click += new System.EventHandler(this.btnAdd_Click);
            // 
            // btnPath
            // 
            this.btnPath.Dock = System.Windows.Forms.DockStyle.Top;
            this.btnPath.FlatAppearance.BorderSize = 0;
            this.btnPath.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnPath.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnPath.Font = new System.Drawing.Font("Microsoft Sans Serif", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnPath.ForeColor = System.Drawing.Color.Gainsboro;
            this.btnPath.IconChar = FontAwesome.Sharp.IconChar.WindowRestore;
            this.btnPath.IconColor = System.Drawing.Color.White;
            this.btnPath.IconSize = 32;
            this.btnPath.Location = new System.Drawing.Point(0, 60);
            this.btnPath.Name = "btnPath";
            this.btnPath.Rotation = 0D;
            this.btnPath.Size = new System.Drawing.Size(151, 60);
            this.btnPath.TabIndex = 20;
            this.btnPath.Text = "Path";
            this.btnPath.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnPath.UseVisualStyleBackColor = true;
            this.btnPath.Click += new System.EventHandler(this.btnPath_Click);
            // 
            // btnRestore
            // 
            this.btnRestore.Dock = System.Windows.Forms.DockStyle.Top;
            this.btnRestore.FlatAppearance.BorderSize = 0;
            this.btnRestore.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnRestore.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnRestore.Font = new System.Drawing.Font("Microsoft Sans Serif", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnRestore.ForeColor = System.Drawing.Color.Gainsboro;
            this.btnRestore.IconChar = FontAwesome.Sharp.IconChar.StarOfLife;
            this.btnRestore.IconColor = System.Drawing.Color.White;
            this.btnRestore.IconSize = 32;
            this.btnRestore.Location = new System.Drawing.Point(0, 120);
            this.btnRestore.Name = "btnRestore";
            this.btnRestore.Rotation = 0D;
            this.btnRestore.Size = new System.Drawing.Size(151, 60);
            this.btnRestore.TabIndex = 21;
            this.btnRestore.Text = "Restore";
            this.btnRestore.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnRestore.UseVisualStyleBackColor = true;
            // 
            // panelMenu
            // 
            this.panelMenu.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(38)))), ((int)(((byte)(70)))), ((int)(((byte)(83)))));
            this.panelMenu.Controls.Add(this.panel5);
            this.panelMenu.Controls.Add(this.panelPictures);
            this.panelMenu.Dock = System.Windows.Forms.DockStyle.Left;
            this.panelMenu.Location = new System.Drawing.Point(0, 0);
            this.panelMenu.Name = "panelMenu";
            this.panelMenu.Size = new System.Drawing.Size(151, 471);
            this.panelMenu.TabIndex = 23;
            // 
            // panel5
            // 
            this.panel5.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.panel5.Controls.Add(this.btnRestore);
            this.panel5.Controls.Add(this.btnPath);
            this.panel5.Controls.Add(this.btnAdd);
            this.panel5.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel5.Location = new System.Drawing.Point(0, 68);
            this.panel5.Name = "panel5";
            this.panel5.Size = new System.Drawing.Size(151, 403);
            this.panel5.TabIndex = 24;
            // 
            // panelPictures
            // 
            this.panelPictures.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(119)))), ((int)(((byte)(182)))));
            this.panelPictures.Controls.Add(this.pictureBox1);
            this.panelPictures.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelPictures.Location = new System.Drawing.Point(0, 0);
            this.panelPictures.Name = "panelPictures";
            this.panelPictures.Size = new System.Drawing.Size(151, 68);
            this.panelPictures.TabIndex = 23;
            // 
            // pictureBox1
            // 
            this.pictureBox1.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.pictureBox1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.pictureBox1.Image = ((System.Drawing.Image)(resources.GetObject("pictureBox1.Image")));
            this.pictureBox1.Location = new System.Drawing.Point(0, 0);
            this.pictureBox1.Name = "pictureBox1";
            this.pictureBox1.Size = new System.Drawing.Size(151, 68);
            this.pictureBox1.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.pictureBox1.TabIndex = 0;
            this.pictureBox1.TabStop = false;
            // 
            // panelTitleBar
            // 
            this.panelTitleBar.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.panelTitleBar.Controls.Add(this.btnMinimize);
            this.panelTitleBar.Controls.Add(this.btnExit);
            this.panelTitleBar.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelTitleBar.Location = new System.Drawing.Point(151, 0);
            this.panelTitleBar.Name = "panelTitleBar";
            this.panelTitleBar.Size = new System.Drawing.Size(967, 68);
            this.panelTitleBar.TabIndex = 24;
            this.panelTitleBar.MouseDown += new System.Windows.Forms.MouseEventHandler(this.panel4_MouseDown);
            // 
            // btnMinimize
            // 
            this.btnMinimize.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.btnMinimize.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnMinimize.ForeColor = System.Drawing.Color.Gainsboro;
            this.btnMinimize.IconChar = FontAwesome.Sharp.IconChar.WindowMinimize;
            this.btnMinimize.IconColor = System.Drawing.Color.Gainsboro;
            this.btnMinimize.IconSize = 23;
            this.btnMinimize.Location = new System.Drawing.Point(893, 12);
            this.btnMinimize.Name = "btnMinimize";
            this.btnMinimize.Size = new System.Drawing.Size(24, 23);
            this.btnMinimize.TabIndex = 7;
            this.btnMinimize.TabStop = false;
            this.btnMinimize.Click += new System.EventHandler(this.btnMinimize_Click);
            // 
            // btnExit
            // 
            this.btnExit.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.btnExit.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnExit.ForeColor = System.Drawing.Color.Gainsboro;
            this.btnExit.IconChar = FontAwesome.Sharp.IconChar.Times;
            this.btnExit.IconColor = System.Drawing.Color.Gainsboro;
            this.btnExit.IconSize = 23;
            this.btnExit.Location = new System.Drawing.Point(923, 12);
            this.btnExit.Name = "btnExit";
            this.btnExit.Size = new System.Drawing.Size(24, 23);
            this.btnExit.TabIndex = 5;
            this.btnExit.TabStop = false;
            this.btnExit.Click += new System.EventHandler(this.btnExit_Click);
            // 
            // bunifuElipse1
            // 
            this.bunifuElipse1.ElipseRadius = 25;
            this.bunifuElipse1.TargetControl = this;
            // 
            // panelMain
            // 
            this.panelMain.Controls.Add(this.btnCancel);
            this.panelMain.Controls.Add(this.btnDifferential);
            this.panelMain.Controls.Add(this.btnIncremental);
            this.panelMain.Controls.Add(this.dataGridView1);
            this.panelMain.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelMain.Location = new System.Drawing.Point(151, 68);
            this.panelMain.Name = "panelMain";
            this.panelMain.Size = new System.Drawing.Size(967, 274);
            this.panelMain.TabIndex = 26;
            // 
            // bunifuElipse3
            // 
            this.bunifuElipse3.ElipseRadius = 35;
            this.bunifuElipse3.TargetControl = this.dataGridView1;
            // 
            // panelBottomBar
            // 
            this.panelBottomBar.Controls.Add(this.rtReport);
            this.panelBottomBar.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panelBottomBar.Location = new System.Drawing.Point(151, 342);
            this.panelBottomBar.Name = "panelBottomBar";
            this.panelBottomBar.Size = new System.Drawing.Size(967, 129);
            this.panelBottomBar.TabIndex = 27;
            // 
            // btnDifferential
            // 
            this.btnDifferential.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.btnDifferential.FlatAppearance.BorderSize = 0;
            this.btnDifferential.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnDifferential.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnDifferential.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnDifferential.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnDifferential.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.btnDifferential.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnDifferential.IconSize = 32;
            this.btnDifferential.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnDifferential.Location = new System.Drawing.Point(758, 197);
            this.btnDifferential.Name = "btnDifferential";
            this.btnDifferential.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.btnDifferential.Rotation = 0D;
            this.btnDifferential.Size = new System.Drawing.Size(176, 60);
            this.btnDifferential.TabIndex = 20;
            this.btnDifferential.Text = "Differential";
            this.btnDifferential.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnDifferential.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnDifferential.UseVisualStyleBackColor = false;
            // 
            // btnIncremental
            // 
            this.btnIncremental.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.btnIncremental.FlatAppearance.BorderSize = 0;
            this.btnIncremental.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnIncremental.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnIncremental.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnIncremental.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnIncremental.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.btnIncremental.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnIncremental.IconSize = 32;
            this.btnIncremental.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnIncremental.Location = new System.Drawing.Point(30, 197);
            this.btnIncremental.Name = "btnIncremental";
            this.btnIncremental.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.btnIncremental.Rotation = 0D;
            this.btnIncremental.Size = new System.Drawing.Size(176, 60);
            this.btnIncremental.TabIndex = 19;
            this.btnIncremental.Text = "Incremental";
            this.btnIncremental.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnIncremental.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnIncremental.UseVisualStyleBackColor = false;
            this.btnIncremental.Click += new System.EventHandler(this.btnIncremental_Click);
            // 
            // btnCancel
            // 
            this.btnCancel.FlatAppearance.BorderSize = 0;
            this.btnCancel.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnCancel.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnCancel.Font = new System.Drawing.Font("Microsoft Sans Serif", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnCancel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnCancel.IconChar = FontAwesome.Sharp.IconChar.Ban;
            this.btnCancel.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnCancel.IconSize = 32;
            this.btnCancel.Location = new System.Drawing.Point(393, 197);
            this.btnCancel.Name = "btnCancel";
            this.btnCancel.Rotation = 0D;
            this.btnCancel.Size = new System.Drawing.Size(151, 60);
            this.btnCancel.TabIndex = 23;
            this.btnCancel.Text = "Cancel";
            this.btnCancel.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnCancel.UseVisualStyleBackColor = true;
            // 
            // Main
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.ClientSize = new System.Drawing.Size(1118, 471);
            this.Controls.Add(this.panelBottomBar);
            this.Controls.Add(this.panelMain);
            this.Controls.Add(this.panelTitleBar);
            this.Controls.Add(this.panelMenu);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.None;
            this.Name = "Main";
            this.Opacity = 0.93D;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Form1";
            this.Load += new System.EventHandler(this.Form1_Load);
            ((System.ComponentModel.ISupportInitialize)(this.dataGridView1)).EndInit();
            this.panelMenu.ResumeLayout(false);
            this.panel5.ResumeLayout(false);
            this.panelPictures.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).EndInit();
            this.panelTitleBar.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.btnMinimize)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.btnExit)).EndInit();
            this.panelMain.ResumeLayout(false);
            this.panelBottomBar.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.DataGridView dataGridView1;
        private System.Windows.Forms.FolderBrowserDialog folderBrowserDialog1;
        private System.Windows.Forms.RichTextBox rtReport;
        private FontAwesome.Sharp.IconButton btnAdd;
        private FontAwesome.Sharp.IconButton btnPath;
        private FontAwesome.Sharp.IconButton btnRestore;
        private System.Windows.Forms.Panel panelMenu;
        private System.Windows.Forms.Panel panelTitleBar;
        private System.Windows.Forms.Panel panel5;
        private System.Windows.Forms.Panel panelPictures;
        private FontAwesome.Sharp.IconPictureBox btnMinimize;
        private FontAwesome.Sharp.IconPictureBox btnExit;
        private System.Windows.Forms.PictureBox pictureBox1;
        private Bunifu.Framework.UI.BunifuElipse bunifuElipse1;
        private System.Windows.Forms.Panel panelMain;
        private Bunifu.Framework.UI.BunifuElipse bunifuElipse3;
        private System.Windows.Forms.Panel panelBottomBar;
        private FontAwesome.Sharp.IconButton btnDifferential;
        private FontAwesome.Sharp.IconButton btnIncremental;
        private FontAwesome.Sharp.IconButton btnCancel;
    }
}

