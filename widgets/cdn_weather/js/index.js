$(document).ready(function() {
    getWeather();
    setInterval("getWeather()", 600000);
});

function getWeather() {
    var iconurl = 'http://www.weather.gc.ca/weathericons/';

    $.ajax({
        crossDomain: true,
        url: 'http://dd.weather.gc.ca/citypage_weather/xml/BC/s0000141_e.xml',
        type: 'GET',
        dataType: "xml",
        success: function(data) {
            $xml = $(data);

            // parse current condition
            $curr = $xml.find('currentConditions');
            $issued = $curr.find('dateTime[zone!="UTC"] > textSummary').text();
            $curcond = $curr.find('condition').text();
            $curtemp = $curr.find('temperature').text();
            $curimg = $curr.find('iconCode').text();

            // parse forecasts
            var forecasts = [];

            $xml.find('forecastGroup > forecast').each(function(index) {
                if (index < 3) {
                    var cast = new Object();
                    cast.time = $(this).find('period').text();
                    cast.cond = $(this).find('abbreviatedForecast > textSummary').text();
                    cast.icon = $(this).find('abbreviatedForecast > iconCode').text();
                    cast.temp = $(this).find('temperatures > temperature').text();

                    forecasts.push(cast);
                }
            });

            // dump data
            $('span.current_cond').text($curcond);
            $('span.current_temp').html($curtemp + '&deg;');
            $('div.img_blk > img').attr("src", iconurl + $curimg + ".gif");

            $('div.next span.time').text(forecasts[0].time);
            $('div.next span.cond').text(forecasts[0].cond);
            $('div.next span.temp').html(forecasts[0].temp + '&deg;');
            $('div.next img').attr("src", iconurl + forecasts[0].icon + ".gif");

            $('div.after span.time').text(forecasts[1].time);
            $('div.after span.cond').text(forecasts[1].cond);
            $('div.after span.temp').html(forecasts[1].temp + '&deg;');
            $('div.after img').attr("src", iconurl + forecasts[1].icon + ".gif");

            $('div.future span.time').text(forecasts[2].time);
            $('div.future span.cond').text(forecasts[2].cond);
            $('div.future span.temp').html(forecasts[2].temp + '&deg;');
            $('div.future img').attr("src", iconurl + forecasts[2].icon + ".gif");

            $('div.issue > span.date').text($issued);
        },
        error: function(jqXHR, textStatus, errorThrown) {
            $('div.issue > span.date').text("Error retrieving xml");
        }
    });
}
