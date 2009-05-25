DELIMITER $$
CREATE PROCEDURE `getCities`(countryCode char(2))
    BEGIN
        SELECT Cities.name FROM Countries, Cities WHERE Cities.countryId = Countries.id AND Countries.code = countryCode;
    END$$
DELIMITER ;


CALL `getCities`('RU');


/* Выбрать все города в стране с заданным кодом, вызов хранимой процедуры  */
$countryCode = 'RU';
mysqli_multi_query($db, "CALL getCities('".mysqli_real_escape_string($db, $countryCode)."');");

mysqli
