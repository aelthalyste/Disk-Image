namespace DiskBackupGUI.Tabs
{
    partial class FormRestore
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
            this.panelMain = new System.Windows.Forms.Panel();
            this.dgwRestore = new System.Windows.Forms.DataGridView();
            this.Column1 = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.Column3 = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.btnVolume = new FontAwesome.Sharp.IconButton();
            this.btnDisc = new FontAwesome.Sharp.IconButton();
            this.bunifuElipse2 = new Bunifu.Framework.UI.BunifuElipse(this.components);
            this.panelMain.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.dgwRestore)).BeginInit();
            this.SuspendLayout();
            // 
            // panelMain
            // 
            this.panelMain.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.panelMain.Controls.Add(this.dgwRestore);
            this.panelMain.Controls.Add(this.btnVolume);
            this.panelMain.Controls.Add(this.btnDisc);
            this.panelMain.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelMain.Location = new System.Drawing.Point(0, 0);
            this.panelMain.Name = "panelMain";
            this.panelMain.Size = new System.Drawing.Size(1054, 436);
            this.panelMain.TabIndex = 28;
            // 
            // dgwRestore
            // 
            this.dgwRestore.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dgwRestore.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.Column1,
            this.Column3});
            this.dgwRestore.Location = new System.Drawing.Point(76, 41);
            this.dgwRestore.Name = "dgwRestore";
            this.dgwRestore.Size = new System.Drawing.Size(904, 166);
            this.dgwRestore.TabIndex = 30;
            this.dgwRestore.CellClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.dgwRestore_CellClick);
            // 
            // Column1
            // 
            this.Column1.HeaderText = "Restore";
            this.Column1.Name = "Column1";
            // 
            // Column3
            // 
            this.Column3.HeaderText = "restoreee";
            this.Column3.Name = "Column3";
            // 
            // btnVolume
            // 
            this.btnVolume.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.btnVolume.FlatAppearance.BorderSize = 0;
            this.btnVolume.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnVolume.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnVolume.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnVolume.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnVolume.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.btnVolume.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnVolume.IconSize = 32;
            this.btnVolume.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnVolume.Location = new System.Drawing.Point(574, 213);
            this.btnVolume.Name = "btnVolume";
            this.btnVolume.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.btnVolume.Rotation = 0D;
            this.btnVolume.Size = new System.Drawing.Size(247, 60);
            this.btnVolume.TabIndex = 21;
            this.btnVolume.Text = "Wipe Volume";
            this.btnVolume.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnVolume.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnVolume.UseVisualStyleBackColor = false;
            this.btnVolume.Click += new System.EventHandler(this.btnVolume_Click);
            // 
            // btnDisc
            // 
            this.btnDisc.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(150)))), ((int)(((byte)(199)))));
            this.btnDisc.FlatAppearance.BorderSize = 0;
            this.btnDisc.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnDisc.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnDisc.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnDisc.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnDisc.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.btnDisc.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnDisc.IconSize = 32;
            this.btnDisc.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnDisc.Location = new System.Drawing.Point(250, 213);
            this.btnDisc.Name = "btnDisc";
            this.btnDisc.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.btnDisc.Rotation = 0D;
            this.btnDisc.Size = new System.Drawing.Size(247, 60);
            this.btnDisc.TabIndex = 27;
            this.btnDisc.Text = "Wipe Disk";
            this.btnDisc.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnDisc.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnDisc.UseVisualStyleBackColor = false;
            this.btnDisc.Click += new System.EventHandler(this.btnDisc_Click);
            // 
            // bunifuElipse2
            // 
            this.bunifuElipse2.ElipseRadius = 35;
            this.bunifuElipse2.TargetControl = this.dgwRestore;
            // 
            // FormRestore
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1054, 436);
            this.Controls.Add(this.panelMain);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.None;
            this.Name = "FormRestore";
            this.Text = "Restore";
            this.panelMain.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.dgwRestore)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Panel panelMain;
        private FontAwesome.Sharp.IconButton btnVolume;
        private FontAwesome.Sharp.IconButton btnDisc;
        private System.Windows.Forms.DataGridView dgwRestore;
        private Bunifu.Framework.UI.BunifuElipse bunifuElipse2;
        private System.Windows.Forms.DataGridViewTextBoxColumn Column1;
        private System.Windows.Forms.DataGridViewTextBoxColumn Column3;
    }
}