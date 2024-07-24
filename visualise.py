import pandas as pd
from matplotlib import pyplot as plt
import numpy as np
import seaborn as sns
import sys, os

import matplotlib.pyplot as plt
import matplotlib as mpl
from matplotlib.ticker import FormatStrFormatter
import argparse

def generate_senders_csv(path, n_senders, file_list):
    path = path
    num_senders = n_senders
    sender_num = 0

    df_sent_cols = [
        "Timestamp",
        "Flow ID",
        "Packet ID",
        "Packet Size",
        "IP ID",
        "DSCP",
        "ECN",
        "TTL",
        "Payload Size",
        "Proto",
        "Source IP",
        "Destination IP",
        "TCP Source Port",
        "TCP Destination Port",
        "TCP Sequence Number",
        "TCP Window Size",
        "Delay",
        "Workload ID",
        "Application ID",
        "Message ID",
    ]

    df_sent_cols_to_drop = [
        0,
        2,
        4,
        6,
        8,
        10,
        12,
        14,
        16,
        18,
        20,
        22,
        24,
        26,
        28,
        30,
        32,
        34,
        36,
        38,
        40,
    ]

    temp_cols = [
        "Timestamp",
        "Flow ID",
        "Packet ID",
        "Packet Size",
        "IP ID",
        "DSCP",
        "ECN",
        "TTL",
        "Payload Size",
        "Proto",
        "Source IP",
        "Destination IP",
        "TCP Source Port",
        "TCP Destination Port",
        "TCP Sequence Number",
        "TCP Window Size",
        "Delay",
        "Workload ID",
        "Application ID",
        "Message ID",
    ]

    temp = pd.DataFrame(columns=temp_cols)
    print(temp.head())

    files = file_list

    for file in files:

        sender_tx_df = pd.read_csv(path + file)
        sender_tx_df = pd.DataFrame(np.vstack([sender_tx_df.columns, sender_tx_df]))
        sender_tx_df.drop(
            sender_tx_df.columns[df_sent_cols_to_drop], axis=1, inplace=True
        )

        sender_tx_df.columns = df_sent_cols
        sender_tx_df["Packet ID"].iloc[0] = 0
        sender_tx_df["Flow ID"].iloc[0] = sender_tx_df["Flow ID"].iloc[1]
        sender_tx_df["IP ID"].iloc[0] = 0
        sender_tx_df["DSCP"].iloc[0] = 0
        sender_tx_df["ECN"].iloc[0] = 0
        sender_tx_df["TCP Sequence Number"].iloc[0] = 0
        # sender_tx_df["TTL"] = sender_tx_df.apply(lambda row: extract_TTL(row['Extra']), axis = 1)
        # sender_tx_df["Proto"] = sender_tx_df.apply(lambda row: extract_protocol(row['Extra']), axis = 1)
        sender_tx_df["Flow ID"] = [sender_num for i in range(sender_tx_df.shape[0])]
        sender_tx_df["Message ID"].iloc[0] = sender_tx_df["Message ID"].iloc[1]

        df_sent_cols_new = [
            "Timestamp",
            "Flow ID",
            "Packet ID",
            "Packet Size",
            "IP ID",
            "DSCP",
            "ECN",
            "Payload Size",
            "TTL",
            "Proto",
            "Source IP",
            "Destination IP",
            "TCP Source Port",
            "TCP Destination Port",
            "TCP Sequence Number",
            "TCP Window Size",
            "Delay",
            "Workload ID",
            "Application ID",
            "Message ID",
        ]
        sender_tx_df = sender_tx_df[df_sent_cols_new]

        # sender_tx_df.drop(['Extra'],axis = 1, inplace=True)
        temp = pd.concat([temp, sender_tx_df], ignore_index=True, copy=False)
        # sender_tx_df.drop(['Extra'],axis = 1, inplace=True)
        save_name = file.split(".")[0] + "_final.csv"
        sender_tx_df.to_csv(path + save_name, index=False)

    # temp.drop(['Extra'],axis = 1, inplace=True)
    print(temp.head())
    print(temp.columns)
    print(temp.shape)

    return temp

def plot_queue(path,queueframe,queuesize=100):

    values = [2, 3]
    dict_switches = {
        2: "A",
        3: "B"
    }

    for value in values:
        bottleneck_source = "/NodeList/{}/DeviceList/0/$ns3::CsmaNetDevice/TxQueue/PacketsInQueue".format(value)
        bottleneck_queue = queueframe[queueframe["source"] == bottleneck_source]
        print(bottleneck_source)

        plt.figure(figsize=(5,5))
        scs = sns.relplot(
            data=bottleneck_queue,
            kind='line',
            x='time',
            y='size',
            legend=False,
        )

        scs.fig.suptitle('Bottleneck queue on switch {} '.format(dict_switches[value]))
        scs.fig.suptitle('Queue on bottleneck switch')
        scs.set(xlabel='Simulation Time (seconds)', ylabel='Queue Size (packets)')
        left = bottleneck_queue["time"].iloc[0]
        right = bottleneck_queue["time"].iloc[-1]
        plt.xlim(left, right)
        #plt.ylim([0,queuesize])
        
        save_name = path + "Queue_profile_on_switch_{}".format(dict_switches[value]) + ".pdf"
        scs.fig.tight_layout()
        plt.savefig(save_name) 

def configure_sender_csv(path, sender_list):

    df = pd.read_csv(path + sender_list[0])
    # Drop columns 1 and 3

    df.drop(df.columns[[0, 2, 4, 6]], axis=1, inplace=True)

    # Rename columns
    df.columns = [
        "Timestamp",
        "Packet Size",
        "Packet ID",
        "TCP Sequence Number",
        "Blank",
    ]

    # Drop the blank column
    df.drop(df.columns[[-1]], axis=1, inplace=True)

    # Save as final csv
    df.to_csv(path + sender_list[0].split(".")[0] + "_final.csv", index=False)

    return df

def timeseries_plot(path, df, start, stop, step=0.005, _bin=0.01):

    # Drop first row
    df = df.iloc[1:]

    e2ed = df["Delay"]
    # Get relative timestamp
    # df["Timestamp"] = df["Timestamp"] - df["Timestamp"].iloc[0]
    time = df["Timestamp"]

    # # Plot timeseries

    # left = df["Timestamp"].iloc[0]
    # right = df["Timestamp"].iloc[-1]

    # left = 2
    # right = 5

    # plt.figure(figsize=(10, 5))
    # plt.plot(time, e2ed, label="End-to-end delay")
    # plt.xlabel("Time (s)")
    # plt.ylabel("Delay (s)")
    # plt.title(f"End-to-end delay vs time")
    # plt.xlim(left, right)
    # plt.legend()
    # plt.savefig(path + f"plot_timeseries_range_{left}_{right}.pdf")

    df["Size"] = df["Packet Size"] * 8

    timestarts = np.arange(start, stop, step)

    rates = []
    for timestart in timestarts:
        timeend = timestart + _bin
        df_time = df[(df["Timestamp"] >= timestart) & (df["Timestamp"] <= timeend)]
        rates.append(df_time["Size"].sum() / _bin / 1e6)

    return time, e2ed, timestarts, rates

def plot_timeseries_per_flow(path, df, start, stop, step=0.005, _bin=0.01, args=None):

    # Split df into flows by application ID
    df = df.iloc[1:]

    # Make a new df for each application ID
    app_ids = df["Application ID"].unique()
    assert len(app_ids) == args.n_senders

    df_dict = {}

    for app_id in app_ids:
        df_app = df[df["Application ID"] == app_id]

        e2ed = df_app["Delay"]
        time = df_app["Timestamp"]

        # # Plot timeseries

        # left = df["Timestamp"].iloc[0]
        # right = df["Timestamp"].iloc[-1]

        # left = 2
        # right = 5

        # plt.figure(figsize=(10, 5))
        # plt.plot(time, e2ed, label="End-to-end delay")
        # plt.xlabel("Time (s)")
        # plt.ylabel("Delay (s)")
        # plt.title(f"End-to-end delay vs time")
        # plt.xlim(left, right)
        # plt.legend()
        # plt.savefig(path + f"plot_timeseries_range_{left}_{right}.pdf")

        df_app["Size"] = df_app["Packet Size"] * 8

        timestarts = np.arange(start, stop, step)

        rates = []
        for timestart in timestarts:
            timeend = timestart + _bin
            df_time = df_app[(df_app["Timestamp"] >= timestart) & (df_app["Timestamp"] <= timeend)]
            rates.append(df_time["Size"].sum() / _bin / 1e6)

        df_dict[app_id] = (time, e2ed, timestarts, rates)

    return df_dict

def plot_sending_rate(df, start, stop, step=0.005, _bin=0.01):

    # df["Timestamp"] = df["Timestamp"] - df["Timestamp"].iloc[0]
    df["Size"] = df["Packet Size"] * 8

    # Bin every 5ms of timestamps and count the number of packets sent in that time
    timestarts = np.arange(start, stop, step)
    # print(timestarts)
    # Get all timestamps from the df which are 0.01 seconds from each time start
    counts = []
    sizes = []
    times = []
    rates = []
    for timestart in timestarts:
        #print("Timestart:", timestart)
        timeend = timestart + _bin
        #print("Timeend:", timeend)
        df_time = df[(df["Timestamp"] >= timestart) & (df["Timestamp"] <= timeend)]
        #print("Number of packets:", len(df_time))
        #print("Total size:", df_time["Packet Size"].sum())
        counts.append(len(df_time))
        sizes.append(df_time["Packet Size"].sum())
        rates.append(df_time["Size"].sum() / _bin / 1e6)

    # # Plot subplot
    # plt.figure(figsize=(10, 5))

    # plt.subplot(1, 2, 1)
    # plt.plot(timestarts, counts, label="Number of packets sent")
    # plt.xlabel("Time (s)")
    # plt.ylabel("Number of packets")
    # plt.title(f"Number of packets sent vs time")
    # plt.legend()

    # plt.subplot(1, 2, 2)
    # plt.plot(timestarts, sizes, label="Total size of packets sent")
    # plt.xlabel("Time (s)")
    # plt.ylabel("Total size of packets (bytes)")
    # plt.title(f"Total size of packets sent vs time")
    # plt.legend()
    # plt.tight_layout()
    # plt.savefig(f"results_test/plot_sending_rate_start_{start}_stop_{stop}.pdf")
    return timestarts, counts, sizes, rates

def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--path", type=str, default="results_test/")
    parser.add_argument("--apprate", type=int, required=True)
    parser.add_argument("--n_senders", type=int, required=True)
    parser.add_argument("--rst", type=bool, default=False)
    parser.add_argument("--infinitedata", type=bool, default=False)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--tcpcc", type=str, default="cubic")
    parser.add_argument("--bw", type=int, default=5)
    parser.add_argument("--qs", type=int, default=10)
    
    return parser.parse_args()

def main():

    args = get_args()
    # Get path
    base_path = args.path

    if args.apprate == 1 and args.n_senders == 1:
        print("1 Mbps base rate for applications, 1 sender flow")
        if not args.infinitedata:
            path = base_path + f"1Mbps_1sender_{args.tcpcc}_{args.seed}_{args.bw}mbps/"
        else:
            path = base_path + f"1Mbps_1sender_inf_{args.tcpcc}_{args.seed}_{args.bw}mbps/"
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 10 and args.n_senders == 1:
        print("10 Mbps base rate for applications, 1 sender flow")
        path = base_path + "10Mbps_1sender/"
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 1 and args.n_senders == 3:
        print("1 Mbps base rate for applications, 3 sender flows")
        if args.infinitedata:
            if args.tcpcc == "bbr":
                path = base_path + f"1Mbps_3senders_inf_rst_bbr_{args.seed}_{args.bw}mbps/"
            else:
                path = base_path + f"1Mbps_3senders_inf_rst_cubic_{args.seed}_{args.bw}mbps/"
            # path = base_path + "1Mbps_3senders_inf/"
        elif args.rst:
            if args.tcpcc == "bbr":
                path = base_path + f"1Mbps_3senders_inf_rst_bbr_{args.seed}_{args.bw}mbps/"
            else:
                path = base_path + f"1Mbps_3senders_inf_rst_cubic_{args.seed}_{args.bw}mbps/"
        else:
            path = base_path + f"1Mbps_3senders_{args.seed}/"
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 10 and args.n_senders == 3:
        print("10 Mbps base rate for applications, 3 sender flows")
        path = base_path + "10Mbps_3senders/"
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 1 and args.n_senders == 6:
        print("1 Mbps base rate for applications, 6 senders flows")
        if args.infinitedata:
            path = base_path + f"1Mbps_6senders_inf_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
        elif args.rst:
            path = base_path + f"1Mbps_6senders_rst_{args.seed}/"
        else:
            path = base_path + "1Mbps_6senders/"
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 10 and args.n_senders == 6:
        print("10 Mbps base rate for applications, 6 senders flows")
        path = base_path + "10Mbps_6senders/"
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 1 and args.n_senders == 30:
        print("1 Mbps base rate for applications, 30 senders flows")
        if not args.rst:
            path = base_path + "1Mbps_30senders/"
        else:
            if args.infinitedata:
                path = base_path + f"1Mbps_60senders_inf_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            else:
                path = base_path + f"1Mbps_60senders_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 1 and args.n_senders == 60:
        print("1 Mbps base rate for applications, 60 senders flows")
        if not args.rst:
            path = base_path + "1Mbps_60senders/"
        else:
            if args.infinitedata:
                path = base_path + f"1Mbps_60senders_inf_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            else:
                path = base_path + f"1Mbps_60senders_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 1 and args.n_senders == 90:
        print("1 Mbps base rate for applications, 90 senders flows")
        if not args.rst:
            path = base_path + "1Mbps_90senders/"
        else:
            if args.infinitedata:
                path = base_path + f"1Mbps_90senders_inf_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            else:
                path = base_path + f"1Mbps_90senders_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 1 and args.n_senders == 120:
        print("1 Mbps base rate for applications, 120 senders flows")
        if not args.rst:
            path = base_path + "1Mbps_120senders/"
        else:
            if args.infinitedata:
                path = base_path + f"1Mbps_120senders_inf_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            else:
                path = base_path + f"1Mbps_120senders_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 1 and args.n_senders == 50:
        print("1 Mbps base rate for applications, 50 senders flows")
        if not args.rst:
            path = base_path + "1Mbps_50senders/"
        else:
            if args.infinitedata:
                path = base_path + f"1Mbps_50senders_inf_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            else:
                path = base_path + f"1Mbps_50senders_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 1 and args.n_senders == 75:
        print("1 Mbps base rate for applications, 75 senders flows")
        if not args.rst:
            path = base_path + "1Mbps_75senders/"
        else:
            if args.infinitedata:
                path = base_path + f"1Mbps_75senders_inf_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            else:
                path = base_path + f"1Mbps_75senders_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 1 and args.n_senders == 80:
        print("1 Mbps base rate for applications, 80 senders flows")
        if not args.rst:
            path = base_path + "1Mbps_80senders/"
        else:
            if args.infinitedata:
                path = base_path + f"1Mbps_80senders_inf_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            else:
                path = base_path + f"1Mbps_80senders_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"

    if args.apprate == 1 and args.n_senders == 100:
        print("1 Mbps base rate for applications, 100 senders flows")
        if not args.rst:
            path = base_path + "1Mbps_100senders/"
        else:
            if args.infinitedata:
                path = base_path + f"1Mbps_100senders_inf_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            else:
                path = base_path + f"1Mbps_100senders_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            
        if not os.path.isdir(path):
            os.mkdir(path)

    if args.apprate == 1 and args.n_senders == 125:
        print("1 Mbps base rate for applications, 125 senders flows")
        if not args.rst:
            path = base_path + "1Mbps_125senders/"
        else:
            if args.infinitedata:
                path = base_path + f"1Mbps_125senders_inf_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            else:
                path = base_path + f"1Mbps_125senders_rst_{args.tcpcc}_{args.seed}_{args.bw}mbps_qs_{args.qs}/"
            
        if not os.path.isdir(path):
            os.mkdir(path)

    # path = "results_test/"
    file_list = [f"small_test_no_disturbance_with_message_ids{args.seed}.csv"]

    if args.n_senders == 1:
        sender_list = [f"small_test_no_disturbance_with_message_ids{args.seed}_sender_4.csv",]
    elif args.n_senders == 3:
        sender_list = [f"small_test_no_disturbance_with_message_ids{args.seed}_sender_4.csv",
                        f"small_test_no_disturbance_with_message_ids{args.seed}_sender_5.csv",
                        f"small_test_no_disturbance_with_message_ids{args.seed}_sender_6.csv",]
    elif args.n_senders == 6:
        sender_list = [f"small_test_no_disturbance_with_message_ids{args.seed}_sender_4.csv",
                        f"small_test_no_disturbance_with_message_ids{args.seed}_sender_5.csv",
                        f"small_test_no_disturbance_with_message_ids{args.seed}_sender_6.csv",
                        f"small_test_no_disturbance_with_message_ids{args.seed}_sender_7.csv",
                        f"small_test_no_disturbance_with_message_ids{args.seed}_sender_8.csv",
                        f"small_test_no_disturbance_with_message_ids{args.seed}_sender_9.csv",]
    else:
        print("Sender number too many to plot")
        pass

    # Check if file 
    file_name = file_list[0].split(".")[0] + "_final.csv"
    file = os.path.join(path, file_name)
    if not os.path.isfile(file):
        print("File does not exist")
        df = generate_senders_csv(path, 1, file_list)
        print("Generated CSV")

    else:
        print("File exists")
        df = pd.read_csv(file)

    print(df.head())

    if args.n_senders == 1 or args.n_senders == 3 or args.n_senders == 6:
        for file in sender_list:
            sender_name = file.split(".")[0] + "_final.csv"
            sender_file = os.path.join(path, sender_name)
            if not os.path.isfile(sender_file):
                print("File does not exist")
                sender_df = configure_sender_csv(path, [file])
                print("Generated CSV")
            else:
                print("File exists")
                sender_df = pd.read_csv(sender_file)
    else:
        pass

    # exit()
    
    start = 1
    stop = 60
    step = 0.005
    _bin = 1

    times, e2ed, timestarts, throughputs = timeseries_plot(path, df, start, stop, step=step, _bin=_bin)

    # Rename queue file
    if not os.path.isfile(path+"queue.csv"):
        os.rename(path+f"small_test_no_disturbance_with_message_ids{args.seed}_queues.csv", path+"queue.csv")
    # Plot queue
    queueframe = pd.read_csv(path + "queue.csv", names=["source", "time", "size"], on_bad_lines='skip')
    # plot_queue(path, queueframe, queuesize=100)

    dropframe = pd.read_csv(path+f"small_test_no_disturbance_with_message_ids{args.seed}_drops.csv",
                             names=["source", "time", "packetsize"])

    if args.n_senders == 1 or args.n_senders == 3 or args.n_senders == 6:
        
        # Plot sending rate
        stimes = []
        scounts = []
        ssizes = []
        srates = []

        for file in sender_list:
            sender_name = file.split(".")[0] + "_final.csv"
            sender_file = os.path.join(path, sender_name)
            sender_df = pd.read_csv(sender_file)
            stime, scount, ssize, srate = plot_sending_rate(sender_df, start, stop, step=step, _bin=_bin)
            stimes.append(stime)
            scounts.append(scount)
            ssizes.append(ssize)
            srates.append(srate)

    else:
        pass

    left = 1
    right = 60
    # top = 150
    # bottom = 0
    # Plot 4 subplots

    if args.n_senders == 6:

        plt.figure(figsize=(10, 16))
        for i in range(0,len(stimes), 3):
            plt.subplot(8, 1, i+1)
            plt.plot(stimes[i], srates[i], label=f"Sending rate App {i}, FB Webserver workload", color="blue")
            # plt.plot(stimes[0], srates[0], label="Sending rate App 1, FB Webserver workload")
            # plt.plot(stimes[1], srates[1], label="Sending rate App 2, DCTCP workload")
            # plt.plot(stimes[2], srates[2], label="Sending rate App 3, FB Hadoop workload")
            plt.xlim(left, right)
            # plt.ylim(bottom, top)
            plt.legend(fontsize=6, loc='upper right')
            plt.xlabel("Time (s)")
            plt.ylabel("Sending rate (Mbps)")
            plt.title(f"Sending rate vs time")
            plt.tight_layout()

            plt.subplot(8, 1, i+2)
            plt.plot(stimes[i+1], srates[i+1], label=f"Sending rate App {i+1}, FB Webserver workload", color="orange")
            plt.xlim(left, right)
            # plt.ylim(bottom, top)
            plt.legend(fontsize=6, loc='upper right')
            plt.xlabel("Time (ms)")
            plt.ylabel("Sending rate (Mbps)")
            plt.title(f"Sending rate vs time")
            plt.tight_layout()

            plt.subplot(8, 1, i+3)
            plt.plot(stimes[i+2], srates[i+2], label=f"Sending rate App {i+2}, FB Webserver workload", color="green")
            plt.xlim(left, right)
            # plt.ylim(bottom, top)
            plt.legend(fontsize=6, loc='upper right')
            plt.xlabel("Time (s)")
            plt.ylabel("Sending rate (Mbps)")
            plt.title(f"Sending rate vs time")
            plt.tight_layout()

        plt.subplot(8, 1, 7)
        # Make e2ed in ms
        e2ed = e2ed * 1000
        plt.plot(times, e2ed, label="End-to-end delay")
        plt.xlabel("Time (s)")
        plt.ylabel("Delay (ms)")
        plt.title(f"End-to-end delay vs time")
        plt.xlim(left, right)
        #plt.ylim(0.03, 0.035)
        plt.legend(fontsize=6, loc='upper right')
        plt.tight_layout()

        # Plot throughput
        plt.subplot(8, 1, 8)
        plt.plot(timestarts, throughputs, label="Throughput", color="red")
        plt.xlabel("Time (s)")
        plt.ylabel("Throughput (Mbps)")
        plt.title(f"Throughput vs time")
        plt.xlim(left, right)
        plt.legend(fontsize=6, loc='upper right')
        plt.tight_layout()
        plt.savefig(path + f"plot_timeseries_start_{left}_stop_{right}.pdf")

    elif args.n_senders == 3:

        plt.figure(figsize=(10, 12))
        for i in range(0,len(stimes), 3):
            plt.subplot(5, 1, i+1)
            plt.plot(stimes[i], srates[i], label=f"Sending rate App {i}, FB Webserver workload", color="blue")
            # plt.plot(stimes[0], srates[0], label="Sending rate App 1, FB Webserver workload")
            # plt.plot(stimes[1], srates[1], label="Sending rate App 2, DCTCP workload")
            # plt.plot(stimes[2], srates[2], label="Sending rate App 3, FB Hadoop workload")
            plt.xlim(left, right)
            # plt.ylim(bottom, top)
            plt.legend(fontsize=6, loc='upper right')
            plt.xlabel("Time (s)")
            plt.ylabel("Sending rate (Mbps)")
            plt.title(f"Sending rate vs time")
            plt.tight_layout()

            plt.subplot(5, 1, i+2)
            plt.plot(stimes[i+1], srates[i+1], label=f"Sending rate App {i+1}, FB Webserver workload", color="orange")
            plt.xlim(left, right)
            # plt.ylim(bottom, top)
            plt.legend(fontsize=6, loc='upper right')
            plt.xlabel("Time (s)")
            plt.ylabel("Sending rate (Mbps)")
            plt.title(f"Sending rate vs time")
            plt.tight_layout()

            plt.subplot(5, 1, i+3)
            plt.plot(stimes[i+2], srates[i+2], label=f"Sending rate App {i+2}, FB Webserver workload", color="green")
            plt.xlim(left, right)
            # plt.ylim(bottom, top)
            plt.legend(fontsize=6, loc='upper right')
            plt.xlabel("Time (s)")
            plt.ylabel("Sending rate (Mbps)")
            plt.title(f"Sending rate vs time")


        plt.subplot(5, 1, 4)
        # Make e2ed in ms
        e2ed = e2ed * 1000
        plt.plot(times, e2ed, label="End-to-end delay")
        plt.xlabel("Time (s)")
        plt.ylabel("Delay (ms)")
        plt.title(f"End-to-end delay vs time")
        plt.xlim(left, right)
        #plt.ylim(0.03, 0.035)
        plt.legend(fontsize=6, loc='upper right')
        plt.tight_layout()

        # Plot throughput
        plt.subplot(5, 1, 5)
        plt.plot(timestarts, throughputs, label="Throughput", color="red")
        plt.xlabel("Time (s)")
        plt.ylabel("Throughput (Mbps)")
        plt.title(f"Throughput vs time")
        plt.xlim(left, right)
        plt.legend(fontsize=6, loc='upper right')
        plt.tight_layout()
        plt.savefig(path + f"plot_timeseries_start_{left}_stop_{right}.pdf")

    elif args.n_senders == 1:

        plt.figure(figsize=(10, 8))
        plt.subplot(3, 1, 1)
        plt.plot(stimes[0], srates[0], label=f"Sending rate App 1, FB Webserver workload", color="blue")
        plt.xlim(left, right)
        # plt.ylim(bottom, top)
        plt.legend(fontsize=6, loc='upper right')
        plt.xlabel("Time (s)")
        plt.ylabel("Sending rate (Mbps)")
        plt.title(f"Sending rate vs time")
        plt.tight_layout()

        plt.subplot(3, 1, 2)
        # Make e2ed in ms
        e2ed = e2ed * 1000
        plt.plot(times, e2ed, label="End-to-end delay")
        plt.xlabel("Time (ms)")
        plt.ylabel("Delay (ms)")
        plt.title(f"End-to-end delay vs time")
        plt.xlim(left, right)
        #plt.ylim(0.03, 0.035)
        plt.legend(fontsize=6, loc='upper right')
        plt.tight_layout()

        # Plot throughput
        plt.subplot(3, 1, 3)
        plt.plot(timestarts, throughputs, label="Throughput", color="red")
        plt.xlabel("Time (s)")
        plt.ylabel("Throughput (Mbps)")
        plt.title(f"Throughput vs time")
        plt.xlim(left, right)
        plt.legend(fontsize=6, loc='upper right')
        plt.tight_layout()
        plt.savefig(path + f"plot_timeseries_start_{left}_stop_{right}.pdf")

    else:
        plt.figure(figsize=(10, 5))

        plt.subplot(1, 2, 1)
        # Make e2ed in ms
        e2ed = e2ed * 1000
        plt.plot(times, e2ed, label="End-to-end delay")
        plt.xlabel("Time (s)")
        plt.ylabel("Delay (ms)")
        plt.title(f"End-to-end delay vs time")
        plt.xlim(left, right)
        #plt.ylim(0.03, 0.035)
        plt.legend(fontsize=6, loc='upper right')
        plt.tight_layout()

        # Plot throughput
        plt.subplot(1, 2, 2)
        plt.plot(timestarts, throughputs, label="Throughput", color="red")
        plt.xlabel("Time (s)")
        plt.ylabel("Throughput (Mbps)")
        plt.title(f"Throughput vs time")
        plt.xlim(left, right)
        plt.legend(fontsize=6, loc='upper right')
        plt.tight_layout()
        plt.savefig(path + f"plot_timeseries_start_{left}_stop_{right}.pdf")

        # Get plot values for each flow
        # df_dict = plot_timeseries_per_flow(path, df, start, stop, step=step, _bin=_bin, args=args)
        # print(len(df_dict))

    print("Drop fraction:", len(dropframe) / (len(dropframe) + len(df)))

    # Plot queue size at receiver
    path = path

    values = [2, 3]
    dict_switches = {
        2: "A",
        3: "B"
    }

    for value in values:
        bottleneck_source = "/NodeList/{}/DeviceList/0/$ns3::CsmaNetDevice/TxQueue/PacketsInQueue".format(value)
        bottleneck_queue = queueframe[queueframe["source"] == bottleneck_source]
        print(bottleneck_source)

        plt.figure(figsize=(5,5))
        scs = sns.relplot(
            data=bottleneck_queue,
            kind='line',
            x='time',
            y='size',
            legend=False,
            errorbar=None,
        )

        scs.fig.suptitle(f'Bottleneck queue on switch {dict_switches[value]} '.format())
        scs.fig.suptitle('Queue on bottleneck switch')
        scs.set(xlabel='Simulation Time (seconds)', ylabel='Queue Size (packets)')
        plt.xlim([0,60])
        plt.ylim([0,200])
        
        save_name = path + f"Queue_profile_on_switch_{dict_switches[value]}" + ".pdf"
        scs.fig.tight_layout()
        plt.savefig(save_name) 

    # Plot the scatter plot of packet drops
    plt.figure(figsize=(5,5))
    # One dimensional scatter plot
    plt.scatter(dropframe["time"], dropframe["packetsize"], s=1, label="Packet drops")
    plt.xlabel("Time (s)")
    plt.ylabel("Packet Size (bytes)")
    plt.title("Packet drops")
    plt.savefig(path + "packet_drops.pdf")



if __name__ == "__main__":
    main()



