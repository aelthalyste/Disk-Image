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
            this.panelTop = new System.Windows.Forms.Panel();
            this.btnOkay = new FontAwesome.Sharp.IconButton();
            this.lblMessage = new Bunifu.Framework.UI.BunifuCustomLabel();
            this.panelBottom = new System.Windows.Forms.Panel();
            this.bunifuElipse1 = new Bunifu.Framework.UI.BunifuElipse(this.components);
            this.ıconButton1 = new FontAwesome.Sharp.IconButton();
            this.SuspendLayout();
            // 
            // panelTop
            // 
            this.panelTop.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.panelTop.Dock = System.Windows.Forms.DockStyle.Top;
            this.panelTop.Location = new System.Drawing.Point(0, 0);
            this.panelTop.Name = "panelTop";
            this.panelTop.Size = new System.Drawing.Size(637, 25);
            this.panelTop.TabIndex = 8;
            // 
            // btnOkay
            // 
            this.btnOkay.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(180)))), ((int)(((byte)(216)))));
            this.btnOkay.FlatAppearance.BorderSize = 0;
            this.btnOkay.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.btnOkay.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.btnOkay.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnOkay.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnOkay.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.btnOkay.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.btnOkay.IconSize = 32;
            this.btnOkay.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnOkay.Location = new System.Drawing.Point(111, 207);
            this.btnOkay.Name = "btnOkay";
            this.btnOkay.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.btnOkay.Rotation = 0D;
            this.btnOkay.Size = new System.Drawing.Size(176, 60);
            this.btnOkay.TabIndex = 7;
            this.btnOkay.Text = "Incremental";
            this.btnOkay.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.btnOkay.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.btnOkay.UseVisualStyleBackColor = false;
            this.btnOkay.Click += new System.EventHandler(this.btnOkay_Click);
            // 
            // lblMessage
            // 
            this.lblMessage.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(180)))), ((int)(((byte)(216)))));
            this.lblMessage.Dock = System.Windows.Forms.DockStyle.Fill;
            this.lblMessage.Font = new System.Drawing.Font("Microsoft Sans Serif", 15.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.lblMessage.Location = new System.Drawing.Point(0, 0);
            this.lblMessage.Name = "lblMessage";
            this.lblMessage.Size = new System.Drawing.Size(637, 267);
            this.lblMessage.TabIndex = 6;
            this.lblMessage.Text = "bunifuCustomLabel1";
            this.lblMessage.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            this.lblMessage.MouseDown += new System.Windows.Forms.MouseEventHandler(this.lblMessage_MouseDown);
            // 
            // panelBottom
            // 
            this.panelBottom.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.panelBottom.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.panelBottom.Location = new System.Drawing.Point(0, 267);
            this.panelBottom.Name = "panelBottom";
            this.panelBottom.Size = new System.Drawing.Size(637, 25);
            this.panelBottom.TabIndex = 5;
            // 
            // bunifuElipse1
            // 
            this.bunifuElipse1.ElipseRadius = 50;
            this.bunifuElipse1.TargetControl = this;
            // 
            // ıconButton1
            // 
            this.ıconButton1.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(180)))), ((int)(((byte)(216)))));
            this.ıconButton1.FlatAppearance.BorderSize = 0;
            this.ıconButton1.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.ıconButton1.Flip = FontAwesome.Sharp.FlipOrientation.Normal;
            this.ıconButton1.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.ıconButton1.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.ıconButton1.IconChar = FontAwesome.Sharp.IconChar.Check;
            this.ıconButton1.IconColor = System.Drawing.Color.FromArgb(((int)(((byte)(2)))), ((int)(((byte)(62)))), ((int)(((byte)(138)))));
            this.ıconButton1.IconSize = 32;
            this.ıconButton1.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.ıconButton1.Location = new System.Drawing.Point(371, 207);
            this.ıconButton1.Name = "ıconButton1";
            this.ıconButton1.Padding = new System.Windows.Forms.Padding(10, 0, 20, 0);
            this.ıconButton1.Rotation = 0D;
            this.ıconButton1.Size = new System.Drawing.Size(176, 60);
            this.ıconButton1.TabIndex = 9;
            this.ıconButton1.Text = "Differential";
            this.ıconButton1.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.ıconButton1.TextImageRelation = System.Windows.Forms.TextImageRelation.ImageBeforeText;
            this.ıconButton1.UseVisualStyleBackColor = false;
            // 
            // MyMessageBox
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(637, 292);
            this.Controls.Add(this.ıconButton1);
            this.Controls.Add(this.panelTop);
            this.Controls.Add(this.btnOkay);
            this.Controls.Add(this.lblMessage);
            this.Controls.Add(this.panelBottom);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.None;
            this.Name = "MyMessageBox";
            this.Opacity = 0.8D;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Form2";
            this.Load += new System.EventHandler(this.MyMessageBox_Load);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Panel panelTop;
        private FontAwesome.Sharp.IconButton btnOkay;
        private Bunifu.Framework.UI.BunifuCustomLabel lblMessage;
        private System.Windows.Forms.Panel panelBottom;
        private Bunifu.Framework.UI.BunifuElipse bunifuElipse1;
        private FontAwesome.Sharp.IconButton ıconButton1;
    }
}