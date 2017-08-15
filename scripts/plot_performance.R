#!/usr/bin/env Rscript
library(optparse) #sudo apt install r-cran-optparse 
library(ggplot2) #sudo apt-get install r-cran-ggplot2

option_list <- list(
  make_option(c("--csvfile"), type="character", default="tmp_data.csv", 
              help="dataset file name", metavar="character"),
	make_option(c("--out"), type="character", default="performance_plot.pdf", 
              help="output file name [default= %default]", metavar="character"),
	make_option(c("--title"), type="character", default=NULL, 
              help="add title to the plot", metavar="character"),
	make_option(c("--ltitle"), type="character", default=NULL, 
              help="add title to the legend", metavar="character"),
	make_option(c("--xlabel"), type="character", default=NULL, 
              help="set label of x-axis", metavar="character"),
	make_option(c("--ylabel"), type="character", default=NULL, 
              help="set label of y-axis", metavar="character"),
	make_option(c("--ydivider"), type="double", default=1, 
              help="set divider for y-axis", metavar="double"),
	make_option(c("--ymin"), type="double", default=NA, 
              help="set limits for y-axis", metavar="double"),
	make_option(c("--ymax"), type="double", default=NA, 
              help="set limits for y-axis", metavar="double"),
	make_option(c("--xmin"), type="double", default=NA, 
              help="set limits for x-axis", metavar="double"),
	make_option(c("--xmax"), type="double", default=NA, 
              help="set limits for x-axis", metavar="double")
); 
 
opt_parser = OptionParser(option_list=option_list);
opt = parse_args(opt_parser);

data <- read.csv(opt$csvfile)
data$yaes <- data$yaes / opt$ydivider
data$err <- data$err / opt$ydivider

plot <- ggplot (data, aes(x=xaes, y=yaes))
#only plot error bars when an error is meassured
if (sum(data$err) > 0) {
    xmin = opt$xmin
    xmax = opt$xmax
    if (is.na(xmin)) {
        xmin = 0  
    }
    if (is.na(xmax)) {
        xmax = sum(!is.na(data$xaes))
    }
    num_points = sum(unique(data$xaes) >= xmin & unique(data$xaes) <= xmax)
    width = num_points * 0.12 
    plot <- plot + geom_errorbar(aes(ymin = yaes + (-err), ymax = yaes + err, color=type), width = width)
}

lim_ymin <- ifelse(is.na(opt$ymin), min(data$yaes), opt$ymin)
lim_ymax <- ifelse(is.na(opt$ymax), max(data$yaes), opt$ymax)
lim_xmin <- ifelse(is.na(opt$xmin), min(data$xaes), opt$xmin)
lim_xmax <- ifelse(is.na(opt$xmax), max(data$xaes), opt$xmax)

plot <- plot + geom_line(aes(color=type), size=0.4) +
    geom_point(aes(color=type, shape=type), size=2) +
    labs(x=opt$xlabel, y=opt$ylabel) +
    scale_color_discrete(name=opt$ltitle) +
    scale_shape_discrete(name=opt$ltitle) +
    coord_cartesian(ylim = c(lim_ymin, lim_ymax), xlim = c(lim_xmin, lim_xmax)) +
    #ylim(opt$ymin,opt$ymax) +
    #xlim(opt$xmin,opt$xmax) +
    theme_bw() 

if (!is.null(opt$title)) {
    plot <- plot + ggtitle(opt$title)    
}

ggsave(opt$out, plot, width=7.2, height=4.6)
