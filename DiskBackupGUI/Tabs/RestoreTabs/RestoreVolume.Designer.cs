namespace DiskBackupGUI.Tabs.RestoreTabs
{
    partial class RestoreVolume
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
            this.bunifuElipse2 = new Bunifu.Framework.UI.BunifuElipse(this.components);
            this.dgwVolume = new System.Windows.Forms.DataGridView();
            this.btnCancel = new FontAwesome.Sharp.IconButton();
            this.panelMain = new System.Windows.Forms.Panel();
            this.btnRestore = new FontAwesome.Sharp.IconButton();
            ((System.ComponentModel.ISupportInitialize)(this.dgwVolume)).BeginInit();
            this.panelMain.SuspendLayout();
            this.SuspendLayout();
            // 
            // bunifuElipse2
            // 
            this.bunifuElipse2.ElipseRadius = 35;
            this.bunifuElipse2.TargetControl = this.dgwVolume;
            // 
            // dgwVolume
            // 
            this.dgwVolume.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dgwVolume.Location = new System.Drawing.Point(76, 41);
            this.dgwVolume.Name = "dgwVolume";
            this.dgwVolume.Size = new System.Drawing.Size(904, 166);
            this.dgwVolume.TabIndex = 2;
            this.dgwVolume.CellClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.dgwVolume_CellClick);
            // 
            // btnCancel
            // 
            this.btnCancel.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.btnCancel.FlatAppearance.BorderSize = 0;
            this.btnCancel.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnCancel.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnCancel.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnCancel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnCancel.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.btnCancel.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnCancel.IconSize = 32;
            this.btnCancel.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnCancel.Location = new System.Drawing.Point(76, 213);
            this.btnCancel.Name = "btnCancel";
            this.btnCancel.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.btnCancel.Rotation = 0D;
            this.btnCancel.Size = new System.Drawing.Size(247, 60);
            this.btnCancel.TabIndex = 28;
            this.btnCancel.Text = "Cancel";
            this.btnCancel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnCancel.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnCancel.UseVisualStyleBackColor = false;
            this.btnCancel.Click += new System.EventHandler(this.btnCancel_Click);
            // 
            // panelMain
            // 
            this.panelMain.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.panelMain.Controls.Add(this.btnRestore);
            this.panelMain.Controls.Add(this.btnCancel);
            this.panelMain.Controls.Add(this.dgwVolume);
            this.panelMain.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelMain.Location = new System.Drawing.Point(0, 0);
            this.panelMain.Name = "panelMain";
            this.panelMain.Size = new System.Drawing.Size(1054, 436);
            this.panelMain.TabIndex = 29;
            // 
            // btnRestore
            // 
            this.btnRestore.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.btnRestore.FlatAppearance.BorderSize = 0;
            this.btnRestore.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnRestore.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnRestore.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnRestore.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnRestore.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.btnRestore.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnRestore.IconSize = 32;
            this.btnRestore.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnRestore.Location = new System.Drawing.Point(752, 213);
            this.btnRestore.Name = "btnRestore";
            this.btnRestore.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.btnRestore.Rotation = 0D;
            this.btnRestore.Size = new System.Drawing.Size(247, 60);
            this.btnRestore.TabIndex = 33;
            this.btnRestore.Text = "Restore";
            this.btnRestore.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnRestore.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnRestore.UseVisualStyleBackColor = false;
            this.btnRestore.Click += new System.EventHandler(this.btnRestore_Click);
            // 
            // RestoreVolume
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1054, 436);
            this.Controls.Add(this.panelMain);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.None;
            this.Name = "RestoreVolume";
            this.Text = "RestoreVolume";
            this.Load += new System.EventHandler(this.RestoreVolume_Load);
            ((System.ComponentModel.ISupportInitialize)(this.dgwVolume)).EndInit();
            this.panelMain.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion
        private Bunifu.Framework.UI.BunifuElipse bunifuElipse2;
        private System.Windows.Forms.DataGridView dgwVolume;
        private FontAwesome.Sharp.IconButton btnCancel;
        private System.Windows.Forms.Panel panelMain;
        private FontAwesome.Sharp.IconButton btnRestore;
    }
}