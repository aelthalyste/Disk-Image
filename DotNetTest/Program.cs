using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using NarDIWrapper;
using System.IO;

namespace DotNetTest
{
	class Program
	{
		static void Main(string[] args)
		{

			FileStream st = File.Create("test");
			DiskTracker tracker = new DiskTracker();

			if (tracker.CW_InitTracker())
			{
				if (tracker.CW_AddToTrack('C', 0))
				{
					for (; ; )
					{
						string Input = Console.ReadLine();
						if (Input == "q")
						{

						}
						else if (Input == "f")
						{
							StreamInfo streamInfo = new StreamInfo();
							if (tracker.CW_SetupStream('C', streamInfo))
							{
								
								unsafe
								{
									var Buffer = new byte[4096];
									fixed (byte* BAddr = &Buffer[0])
									{

										if(tracker.CW_ReadStream(BAddr, 4096))
										{
											st.Write(Buffer, 0, 4096);
										}

									}
								}

							}


						}
						else if (Input == "d")
						{

						}

					}
				}
				else
				{
					// TODO err
				}
			}
			else
			{
				// TODO err
			}

		}


	}
}
