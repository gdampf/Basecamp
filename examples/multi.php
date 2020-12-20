<?php
function myErrorHandler($fehlercode, $fehlertext, $fehlerdatei, $fehlerzeile)
{
    if (!(error_reporting() & $fehlercode)) {
        // Dieser Fehlercode ist nicht in error_reporting enthalten
        return;
    }

    switch ($fehlercode) {
    case E_USER_ERROR:
        echo "<b>Mein FEHLER</b> [$fehlercode] $fehlertext<br />\n";
        echo "  Fataler Fehler in Zeile $fehlerzeile in der Datei $fehlerdatei";
        echo ", PHP " . PHP_VERSION . " (" . PHP_OS . ")<br />\n";
        echo "Abbruch...<br />\n";
        exit(1);
        break;

    case E_USER_WARNING:
        echo "<b>Meine WARNUNG</b> [$fehlercode] $fehlertext<br />\n";
        break;

    case E_USER_NOTICE:
        echo "<b>Mein HINWEIS</b> [$fehlercode] $fehlertext<br />\n";
        break;

    default:
        echo "Unbekannter Fehlertyp: [$fehlercode] $fehlertext<br />\n";
        break;
    }

    /* Damit die PHP-interne Fehlerbehandlung nicht ausgeführt wird */
    return true;
}

set_error_handler("myErrorHandler");

setlocale (LC_ALL, 'de_DE.UTF-8');
$url = "/var/www/html/contents/weather_".$location.".json";

//Using Weather-Icons by Erik Flowers. All codes can be found here: https://github.com/erikflowers/weather-icons/blob/master/css/weather-icons.css
const ICONS = array(
"01d"=>'&#xf00d;',
"01n"=>'&#xf02e;',
"02d"=>'&#xf00c;',
"02n"=>'&#xf081;',
"03d"=>'&#xf041;',
"03n"=>'&#xf041;',
"04d"=>'&#xf013;',
"04n"=>'&#xf013;',
"09d"=>'&#xf01a;',
"09n"=>'&#xf01a;',
"10d"=>'&#xf008;',
"10n"=>'&#xf028;',
"11d"=>'&#xf01e;',
"11n"=>'&#xf01e;',
"13d"=>'&#xf01b;',
"13n"=>'&#xf01b;',
"50d"=>'&#xf014;',
"50n"=>'&#xf014;',
"default"=>'&#xf07b;');

const MEASURE = array(
"temp"=>'°',
"humidity"=>'%');

try {
  $contents = file_get_contents($url);
  $results = json_decode($contents,true); 

  for ($i = 0; $i < 5; $i++)
  {
    if (!isset($temp_min) || $temp_min > $results['list'][$i]['main']['temp']) $temp_min = $results['list'][$i]['main']['temp'];
    if (!isset($temp_max) || $temp_max < $results['list'][$i]['main']['temp']) $temp_max = $results['list'][$i]['main']['temp'];
  }
  $temp_min -= 273.15;
  $temp_max -= 273.15;

  $humidity = $results['list'][1]['main']['humidity'];
  $pressure = round($results['list'][1]['main']['pressure']);
  $wind = round($results['list'][1]['wind']['speed'] * 10)/10;
  $icon = $results['list'][1]['weather'][0]['icon'];

  $fontSize = $scale;

  //Write weather for today
  $icon = ICONS[$icon];
  if (is_null($icon)) $icon = ICONS['default'];
  imagettftext($im, $scale*4, 0, 10, $scale*6, $black, realpath("./fonts/weathericons-regular-webfont.ttf"), $icon);
  imagettftext($im, $scale*4, 0, $scale*10, $scale*5, $black, $DEFAULT_FONT['regular'], number_format($temp_min)."°C/".number_format($temp_max)."°C");

  //Write additional
  imagettftext($im, $scale*2, 0, $scale*5, $scale*9, $black, $DEFAULT_FONT['regular'], $pressure." hpa");
  imagettftext($im, $scale*2, 0, $scale*18, $scale*9, $black, $DEFAULT_FONT['regular'], $humidity."%");
  imagettftext($im, $scale*2, 0, $scale*25, $scale*9, $black, $DEFAULT_FONT['regular'], $wind." m/s");
  imagettftext($im, $scale, 0, $scale*2, $scale*11, $black, $DEFAULT_FONT['regular'], strftime('%a. %d.%m.%Y %H:%M'));

  $sx = imagesx($im);
  $sy = imagesy($im);
  $sa = $scale*12;

  $url = "/var/www/html/contents/data.json";
  $contents = file_get_contents($url);
  $results = json_decode($contents);
  $anz_cl = sizeof($results->{'trow'});
  if ($anz_cl) $wdt = intdiv($sx, $anz_cl)-10;
  else $wdt = $sx;

  $min = mktime();
  $max = 0;
  $maxval = array();
  $minval = array();

  foreach ($results->{'trow'} as &$client) {
    foreach ($client->{'data'} as &$dpoint) {
      foreach ($dpoint as $key => $value) {
        if ($key == 'ts') {
          if ($min > $value) $min = $value;
          if ($max < $value) $max = $value;
        } else
        {
          if (!isset($maxval[$key]) || $maxval[$key] < $value) $maxval[$key] = $value;
          if (!isset($minval[$key]) || $minval[$key] > $value) $minval[$key] = $value;
        }
      }
    }
  }

  foreach ($maxval as $key => $value) $maxval[$key] = 10*ceil($value*1.01/10);
  foreach ($minval as $key => $value) $minval[$key] = 10*floor($value*0.99/10);

  $legend = "";
  foreach ($maxval as $key => $value) {
    if ($legend != "") $legend .= " / ";
    if (isset($minval[$key])) $legend .= $minval[$key]."-";
    if (isset(MEASURE[$key])) $legend .= $value.MEASURE[$key];
    else $legend .= $value." ".$key;
  }
  imagettftext($im, $scale*2/5, 0, 0, $sa, $black, $DEFAULT_FONT['regular'], $legend);

  $scalex = $wdt/($max-$min);
  $scaley = ($sy-$sa-10);
  $i = 0;

  foreach ($results->{'trow'} as &$client) {
    $sb = $i * ($wdt+10) + 5;
    imageline($im, $sb, $sa, $sb, $sy, $black);
    imageline($im, $sb, $sy-10, $sb+$wdt, $sy-10, $black);
    imagettftext($im, $scale*2/5, 0, $sb+2, $sy-1, $black, $DEFAULT_FONT['regular'], strftime('%H:%M',$min));
    imagettftext($im, $scale*2/5, 0, $sb+$wdt/2, $sy-1, $black, $DEFAULT_FONT['regular'], $client->{'id'});
    imagettftext($im, $scale*2/5, 0, $sb+$wdt-$scale*6/5, $sy-1, $black, $DEFAULT_FONT['regular'], strftime('%H:%M',$max));

    $keys = array();
    foreach ($client->{'data'} as &$dpoint) $keys = array_merge($keys,(array)$dpoint);
    $keys = array_keys($keys);
    $j = array_search("ts",$keys);
    if (is_int($j)) unset($keys[$j]);

    $j = 0;

    foreach ($keys as &$key) 
      if (isset($minval[$key]) && isset($maxval[$key]))
      {
        $tick = array();
        $xx = NULL; $xa = NULL; $ya = NULL; $vv = NULL;
        foreach ($client->{'data'} as &$dpoint) {
          $v = ((array)$dpoint)[$key];
          if (is_null($v)) continue;
          $vv = $v;
          $v -= $minval[$key];
          $xe = round($sb + ($dpoint->{'ts'}-$min)*$scalex);
          $ye = round($sy -10 - $v*$scaley/($maxval[$key]-$minval[$key]));
          if (count($tick) == 0 || $xx == $xe) {
            $tick[] = $ye;
            $xx = $xe;
          } else {
            $yy = round(array_sum($tick) / count($tick));
            if (is_null($xa)) {
              $xa = $xx;
              $ya = $yy;
            } else {
              imageline($im, $xa, $ya, $xx, $yy, $black);
              $xa = $xx;
              $ya = $yy;
            }
            $tick = array($ye);
            $xx = $xe;
          }
        }
        if (count($tick) > 0 && !is_null($xa)) imageline($im,$xa,$ya,$xx,round(array_sum($tick) / count($tick)),$black);

        if (!is_null($vv)) {
          if (isset(MEASURE[$key])) $legend = number_format($vv,1,',','.').MEASURE[$key];
          else $legend = $vv." ".$key;
          $w = imagettfbbox($scale*2/5,0, $DEFAULT_FONT['regular'], $legend)[2];
          $xa = $xe-$w+7;
          if ($xa < $sb+2) $xa = $sb+2;
          imagettftext($im, $scale*2/5, 0, $xa, $sa+($j++ * $scale/2), $black, $DEFAULT_FONT['regular'], $legend);
        }
      }
      else imagettftext($im, $scale*2/5, 0, $sb+$wdt/2+10, $sy-1, $black, $DEFAULT_FONT['regular'], "Min-Max-Fehler");
    $i++;
  }
} catch (Exception  $e) {
  echo 'Exception abgefangen: ',  $e->getMessage(), "\n";
}
